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

static QueueHandle_t commandQueue = nullptr;
static QueueHandle_t telegramMessageQueue = nullptr;

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

   commandQueue = xQueueCreate(8, sizeof(MonitorCommand));
   telegramMessageQueue = xQueueCreate(4, sizeof(TelegramMessage));

   if (commandQueue == nullptr) {
      DBG("ERRO ao criar CommandQueue\n");
      return;
   }
   DBG("CommandQueue criada com sucesso\n");

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

      // Envia mensagens produzidas pelo Monitor
      TelegramMessage message = {};

      while (getTelegramMessage(message)) {

         DBG("Mensagem retirada da fila do Telegram: %s\n", message.text);

         if (!telegramSendMessage(String(message.text))) {
            DBG("Falha ao enviar mensagem da fila do Telegram\n");
            break;
         }
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

      // CommandResult cmdResult = telegramProcessCommand(upd.text);

      // if (!cmdResult.message.isEmpty()) {

      //    DBG("Vai enviar resposta ao comando...\n");

      //    if (!telegramSendMessage(cmdResult.message)) {
      //       DBG("Falha ao enviar resposta ao comando\n");

      //       telegramBusy = false;
      //       continue;
      //    }
      // }

      // if (cmdResult.deferredFunction != nullptr) {
      //    DBG("Executando deferredFunction\n");
      //    cmdResult.deferredFunction();
      // }

      // Só libera depois de terminar tudo, inclusive OTA.
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

   if (commandQueue == nullptr)
      return false;

   MonitorCommand command = {};

   strncpy(command.text, text, sizeof(command.text) - 1);
   command.text[sizeof(command.text) - 1] = '\0';

   DBG("Comando colocado na fila: %s\n", command.text);

   return xQueueSend(commandQueue, &command, 0) == pdPASS;
}

bool getMonitorCommand(MonitorCommand &command) {

   if (commandQueue == nullptr)
      return false;

   return xQueueReceive(commandQueue, &command, 0) == pdPASS;
}

bool queueTelegramMessage(const char *text) {
   if (telegramMessageQueue == nullptr)
      return false;

   TelegramMessage message = {};

   strncpy(message.text, text, sizeof(message.text) - 1);
   message.text[sizeof(message.text) - 1] = '\0';

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
