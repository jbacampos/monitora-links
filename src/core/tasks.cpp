#include "core/tasks.h"
#include "config/config.h"
#include "hardware/led.h"

#include <Arduino.h>
#include <core/storage.h>
#include <network/telegram.h>
#include <network/wifi_manager.h>

// tasks.cpp

static TaskHandle_t ledTaskHandle = nullptr;
static TaskHandle_t telegramTaskHandle = nullptr;
static SemaphoreHandle_t runtimeMutex = nullptr;

static QueueHandle_t telegramMessageQueue = nullptr;
static QueueHandle_t monitorCommandQueue = nullptr;
static QueueHandle_t monitorActionQueue = nullptr;

static volatile bool serviceWindowOpen = false;
static volatile bool telegramBusy = false;
static void telegramTask(void *parameter);

//=============================================================================
// Led
//=============================================================================

static void ledStartupSequence() {

   const LedColor colors[] = {
      LED_RED,
      LED_BLUE,
      LED_YELLOW,
      // LED_CYAN,
      LED_MAGENTA,
      LED_GREEN,
      // LED_WHITE
   };

   // DBG("Iniciando sequência de LEDs\n");

   for (LedColor color : colors) {
      for (uint8_t n = 0; n < 1; n++) {
         setColor(color);
         vTaskDelay(pdMS_TO_TICKS(1000));

         setColor(LED_BLANK);
         vTaskDelay(pdMS_TO_TICKS(100));
      }
   }
}

static void ledTask(void *parameter) {

   // Sinalização visual de inicialização
   ledStartupSequence();

   // Funcionamento normal
   DBG("Task do LED iniciada no core %d\n", xPortGetCoreID());

   for (;;) {
      ledUpdate();
      vTaskDelay(pdMS_TO_TICKS(20));
   }
}

void initLedTask() {

   BaseType_t result = xTaskCreatePinnedToCore(ledTask, "LedTask", 4096, nullptr, 1, &ledTaskHandle, 0);

   if (result == pdPASS)
      DBG("Task do LED criada com sucesso\n");
   else
      DBG("ERRO ao criar task do LED\n");
}

//=============================================================================
// Telegram
//=============================================================================

void initTelegramTask() {

   telegramMessageQueue = xQueueCreate(4, sizeof(TelegramMessage));
   if (telegramMessageQueue == nullptr) {
      DBG("ERRO ao criar telegramMessageQueue\n");
      return;
   }
   DBG("telegramMessageQueue criada com sucesso\n");

   monitorCommandQueue = xQueueCreate(8, sizeof(MonitorCommand));
   if (monitorCommandQueue == nullptr) {
      DBG("ERRO ao criar monitorCommandQueue\n");
      return;
   }
   DBG("monitorCommandQueue criada com sucesso\n");

   monitorActionQueue = xQueueCreate(1, sizeof(MonitorAction));
   if (monitorActionQueue == nullptr) {
      DBG("ERRO ao criar monitorActionQueue\n");
      return;
   }

   DBG("monitorActionQueue criada com sucesso\n");

   BaseType_t result = xTaskCreatePinnedToCore(telegramTask, "TelegramTask", 8192, nullptr, 1, &telegramTaskHandle, 0);

   if (result == pdPASS)
      DBG("Task do Telegram criada com sucesso\n");
   else
      DBG("ERRO ao criar task do Telegram\n");
}

static void telegramTask(void *parameter) {

   DBG("Task do Telegram iniciada no core %d\n", xPortGetCoreID());

   TelegramUpdate upd;
   uint32_t proximoGetUpdates = 0;

   for (;;) {

      // DBG("TelegramTask: serviceWindowOpen=%d WiFi=%d\n", serviceWindowOpen, WiFi.status());
      if (!serviceWindowOpen || WiFi.status() != WL_CONNECTED) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      if (millis() < proximoGetUpdates) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      // A partir daqui, a task está usando a conexão.
      telegramBusy = true;

      bool actionRequested = false;
      
      // Envia mensagens produzidas pelo Monitor
      TelegramMessage message = {};

      while (getTelegramMessage(message)) {

         DBG("Mensagem retirada da fila do Telegram: %s\n", message.text);

         if (!telegramSendMessage(String(message.text))) {
            DBG("Falha ao enviar mensagem da fila do Telegram\n");
            break;
         }

         if (message.action == TELEGRAM_ACTION_REBOOT) {
            DBG("Mensagem enviada. Solicitando reboot ao Monitor.\n");
            if (queueMonitorAction(ACTION_REBOOT))
               actionRequested = true;
            else
               DBG("ERRO ao colocar ACTION_REBOOT na fila\n");

         }

         if (message.action == TELEGRAM_ACTION_OTA) {
            DBG("Mensagem enviada. Solicitando OTA ao Monitor.\n");
            if (queueMonitorAction(ACTION_OTA))
               actionRequested = true;
            else
               DBG("ERRO ao colocar ACTION_OTA na fila\n");
         }
      }

      if (actionRequested) {
         telegramBusy = false;
         proximoGetUpdates = millis() + TELEGRAM_GET_UPDATES_INTERVAL;
         continue;
      }      
      
      // Envia primeiro as notificações pendentes
      if (hasPendingNotifications()) {
         DBG("TelegramTask: verificando notificacoes pendentes\n");
         sendPendingNotifications();
      }

      proximoGetUpdates = millis() + TELEGRAM_GET_UPDATES_INTERVAL;

      if (!telegramGetUpdates(&upd)) {
         telegramBusy = false;
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      DBG("Update recebido = %u - %s\n", upd.updateId, upd.text.c_str());

      lockRuntime();
      gRuntime.telegramUpdateId = upd.updateId;
      gRuntime.saveCount++;
      saveStorage(FILE_RUNTIME, gRuntime);
      unlockRuntime();

      if (!isAuthorizedChat(upd.chatId)) {
         telegramSendMessage("⛔ Chat não autorizado.\nUse o MonitLinks");

         telegramBusy = false;
         continue;
      }

      if (upd.text.isEmpty()) {
         telegramBusy = false;
         continue;
      }

      DBG("upd.text = %s\n", upd.text.c_str());

      if (!queueMonitorCommand(upd.text.c_str())) {
         DBG("Falha ao colocar comando na fila: %s\n", upd.text.c_str());
      }

      // Libera a conexão para o próximo ciclo
      telegramBusy = false;
   }
}

void openServiceWindow() { serviceWindowOpen = true; }

void closeServiceWindow() {

   // Impede que a TelegramTask inicie uma nova operação.
   serviceWindowOpen = false;

   // Se ela já estava trabalhando, espera terminar.
   while (telegramBusy) {
      vTaskDelay(pdMS_TO_TICKS(10));
   }
}

//=============================================================================
// Runtime Mutex
//=============================================================================

void initRuntimeMutex() {

   runtimeMutex = xSemaphoreCreateMutex();

   // if (runtimeMutex != nullptr)
   //    DBG("Mutex do runtime criado\n");
   // else
   //    DBG("ERRO ao criar mutex do runtime\n");
}

void lockRuntime() {

   // DBG(">>> lockRuntime: tentando\n");

   if (runtimeMutex != nullptr)
      xSemaphoreTake(runtimeMutex, portMAX_DELAY);

   // DBG(">>> lockRuntime: conseguiu\n");
}

void unlockRuntime() {

   // DBG("<<< unlockRuntime\n");

   if (runtimeMutex != nullptr)
      xSemaphoreGive(runtimeMutex);
}

bool queueMonitorCommand(const char *text) {

   if (monitorCommandQueue == nullptr)
      return false;

   MonitorCommand command = {};

   strncpy(command.text, text, sizeof(command.text) - 1);
   command.text[sizeof(command.text) - 1] = '\0';

   DBG("Comando colocado na fila: %s\n", command.text);

   return xQueueSend(monitorCommandQueue, &command, 0) == pdPASS;
}

bool getMonitorCommand(MonitorCommand &command) {

   if (monitorCommandQueue == nullptr)
      return false;

   return xQueueReceive(monitorCommandQueue, &command, 0) == pdPASS;
}

bool queueTelegramMessage(const char *text, TelegramAction action) {
   
   if (telegramMessageQueue == nullptr)
      return false;

   TelegramMessage message = {};

   strncpy(message.text, text, sizeof(message.text) - 1);
   message.text[sizeof(message.text) - 1] = '\0';

   message.action = action;

   DBG("Mensagem do Telegram colocada na fila: %s\n", message.text);

   return xQueueSend(telegramMessageQueue, &message, 0) == pdPASS;
}

bool getTelegramMessage(TelegramMessage &message) {
   if (telegramMessageQueue == nullptr)
      return false;

   if (xQueueReceive(telegramMessageQueue, &message, 0) != pdPASS)
      return false;

   DBG("Mensagem do Telegram retirada da fila: %s\n", message.text);

   return true;
}


bool queueMonitorAction(MonitorAction action) {

   if (monitorActionQueue == nullptr)
      return false;

   return xQueueSend(monitorActionQueue, &action, 0) == pdPASS;
}

bool getMonitorAction(MonitorAction &action) {

   if (monitorActionQueue == nullptr)
      return false;

   return xQueueReceive(monitorActionQueue, &action, 0) == pdPASS;
}