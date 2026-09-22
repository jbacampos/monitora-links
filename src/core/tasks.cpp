#include "core/tasks.h"
#include "config/config.h"
#include "hardware/led.h"

#include <Arduino.h>
#include <core/storage.h>
#include <network/telegram.h>
#include <network/wifi_manager.h>

#include <freertos/semphr.h>
#include <freertos/task.h>

// tasks.cpp

static TaskHandle_t ledTaskHandle = nullptr;
static TaskHandle_t telegramTaskHandle = nullptr;
static SemaphoreHandle_t runtimeMutex = nullptr;
static SemaphoreHandle_t configMutex = nullptr;

static QueueHandle_t telegramMessageQueue = nullptr;
static QueueHandle_t monitorCommandQueue = nullptr;
static QueueHandle_t monitorActionQueue = nullptr;

// Contador de tentativas de envio do item que está no topo da fila de
// mensagens. Só a TelegramTask consome telegramMessageQueue, então este
// contador não precisa de sincronização.
static uint8_t telegramMessageAttempts = 0;

static bool discardTelegramMessage(TelegramMessage &reuse);

static void telegramTask(void *parameter);
static void telegramServiceRound(uint32_t &proximoGetUpdates);

//=============================================================================
// Janela de serviço
//
// O Monitor é dono do rádio e empresta a conexão Wi-Fi para a TelegramTask
// durante a "janela de serviço". Invariante: se closeServiceWindow() retorna
// true, a TelegramTask não está (e não entrará) em nenhuma operação de
// rede/flash até uma nova chamada de openServiceWindow().
//
// Pré-condição de uso: serviceWindowReady == true. Os dois objetos são
// criados de forma "tudo ou nada": se um falhar, o outro é destruído e o
// protocolo fica desabilitado (o Monitor continua funcionando sem Telegram).
//=============================================================================

static SemaphoreHandle_t serviceMutex = nullptr;     // protege o estado abaixo
static SemaphoreHandle_t telegramIdleSem = nullptr;  // sinalizado a cada liberação
static bool serviceWindowReady = false;              // os DOIS objetos existem?
static bool serviceWindowOpen = false;               // Monitor autoriza o uso?
static bool telegramBusy = false;                    // TelegramTask está usando?

static bool acquireServiceWindow();
static void releaseServiceWindow();

static void acknowledgeTelegramUpdate(uint32_t updateId) {
   lockRuntime();
   gRuntime.telegramUpdateId = updateId;
   gRuntime.saveCount++;
   saveStorage(FILE_RUNTIME, gRuntime);
   unlockRuntime();
}

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

   uint32_t proximoGetUpdates = 0;

   for (;;) {

      if (millis() < proximoGetUpdates) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      // Daqui até releaseServiceWindow() esta task é a única usuária do
      // rádio, e o Monitor enxerga isso (telegramBusy) de forma atômica.
      if (!acquireServiceWindow()) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      telegramServiceRound(proximoGetUpdates);   // pode dar return em qualquer ponto

      releaseServiceWindow();                    // ÚNICO ponto de liberação
   }
}

// Uma rodada de serviço. Sai por return em qualquer ponto; a liberação da
// conexão é responsabilidade do chamador (telegramTask).
static void telegramServiceRound(uint32_t &proximoGetUpdates) {

   TelegramUpdate upd;

   bool actionRequested = false;
      
      // Envia mensagens produzidas pelo Monitor
      TelegramMessage message = {};

      while (getTelegramMessage(message)) {

         DBG("Mensagem retirada da fila do Telegram: %s\n", message.text);

         if (!telegramSendMessage(String(message.text))) {

            if (++telegramMessageAttempts < TELEGRAM_MESSAGE_MAX_ATTEMPTS) {
               DBG("Falha ao enviar. Nova tentativa na proxima rodada (%u/%u)\n",
                   (unsigned)telegramMessageAttempts, (unsigned)TELEGRAM_MESSAGE_MAX_ATTEMPTS);
            } else {
               DBG("Mensagem descartada apos %u tentativas\n", (unsigned)telegramMessageAttempts);
               discardTelegramMessage(message);
            }

            break;      // nao alonga a janela de servico
         }

         discardTelegramMessage(message);   // enviada: agora sai da fila

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
         proximoGetUpdates = millis() + TELEGRAM_GET_UPDATES_INTERVAL;
         return;
      }      
      
      // Envia primeiro as notificações pendentes
      if (hasPendingNotifications()) {
         DBG("TelegramTask: verificando notificacoes pendentes\n");
         sendPendingNotifications();
      }

      proximoGetUpdates = millis() + TELEGRAM_GET_UPDATES_INTERVAL;

      if (!telegramGetUpdates(&upd))
         return;

      if (upd.updateId == 0 || upd.chatId.isEmpty() || upd.text.isEmpty()) {
         DBG("Telegram: update vazio ou inválido - ignorando.\n");
         if (upd.updateId != 0)
            acknowledgeTelegramUpdate(upd.updateId);
         return;
      }

      DBG("Update recebido = %u - %s\n", upd.updateId, upd.text.c_str());

      if (!isAuthorizedChat(upd.chatId)) {
         telegramSendMessage("⛔ Chat não autorizado.\nUse o MonitLinks");
         acknowledgeTelegramUpdate(upd.updateId);

         return;
      }

      DBG("upd.text = %s\n", upd.text.c_str());

      if (!queueMonitorCommand(upd.text.c_str())) {
         DBG("Falha ao colocar comando na fila: %s\n", upd.text.c_str());
      } else {
         acknowledgeTelegramUpdate(upd.updateId);
      }
}

//=============================================================================
// Janela de serviço
//=============================================================================

void initServiceWindow() {

   if (serviceWindowReady)
      return;

   // Tudo ou nada: o protocolo depende dos DOIS objetos. Se apenas um fosse
   // criado, acquireServiceWindow() poderia tomar o "lease" sem que
   // releaseServiceWindow() conseguisse sinalizar telegramIdleSem -- e o
   // closeServiceWindow() acabaria usando um handle inválido (assert/panic).
   serviceMutex = xSemaphoreCreateMutex();
   telegramIdleSem = xSemaphoreCreateBinary();

   if (serviceMutex == nullptr || telegramIdleSem == nullptr) {

      if (serviceMutex != nullptr) {
         vSemaphoreDelete(serviceMutex);
         serviceMutex = nullptr;
      }

      if (telegramIdleSem != nullptr) {
         vSemaphoreDelete(telegramIdleSem);
         telegramIdleSem = nullptr;
      }

      serviceWindowReady = false;
      DBG("ERRO ao criar a janela de serviço. Telegram desabilitado.\n");
      return;
   }

   serviceWindowReady = true;
   DBG("Janela de serviço inicializada\n");
}

// Só a TelegramTask chama. Devolve true já com o "lease" tomado.
static bool acquireServiceWindow() {

   if (!serviceWindowReady)
      return false;                 // modo degradado: sem empréstimo

   bool pode = false;

   xSemaphoreTake(serviceMutex, portMAX_DELAY);

   // Leitura do estado + marcação de "ocupado" na MESMA região crítica:
   // é isso que elimina a janela entre ler serviceWindowOpen e marcar
   // telegramBusy.
   if (serviceWindowOpen && WiFi.status() == WL_CONNECTED) {
      telegramBusy = true;
      pode = true;
   }

   xSemaphoreGive(serviceMutex);

   return pode;
}

// Só a TelegramTask chama. Único ponto que baixa a flag e acorda o Monitor.
static void releaseServiceWindow() {

   if (!serviceWindowReady)
      return;

   xSemaphoreTake(serviceMutex, portMAX_DELAY);
   telegramBusy = false;                       // 1) estado
   xSemaphoreGive(serviceMutex);

   xSemaphoreGive(telegramIdleSem);            // 2) evento (nunca bloqueia)
}

void openServiceWindow() {

   if (!serviceWindowReady)
      return;

   xSemaphoreTake(serviceMutex, portMAX_DELAY);
   serviceWindowOpen = true;
   xSemaphoreGive(serviceMutex);
}

// Retorna true somente se o rádio está comprovadamente livre para o Monitor.
bool closeServiceWindow() {

   if (!serviceWindowReady)
      return true;                 // ninguém pode estar usando o rádio

   // Fecha a janela e lê o estado de forma atômica (não é mais TOCTOU).
   xSemaphoreTake(serviceMutex, portMAX_DELAY);
   serviceWindowOpen = false;
   bool busy = telegramBusy;
   xSemaphoreGive(serviceMutex);

   if (!busy) {
      xSemaphoreTake(telegramIdleSem, 0);      // descarta evento obsoleto
      return true;
   }

   // Defesa em profundidade: com o init "tudo ou nada" isto não deveria
   // ocorrer; se ocorrer, adiamos o ciclo em vez de usar handle inválido.
   if (telegramIdleSem == nullptr) {
      DBG("ERRO: telegramIdleSem inexistente em closeServiceWindow()\n");
      return false;
   }

   DBG("closeServiceWindow(): aguardando TelegramTask liberar a conexão...\n");

   const TickType_t inicio = xTaskGetTickCount();
   const TickType_t limite = pdMS_TO_TICKS(SERVICE_WINDOW_CLOSE_TIMEOUT_MS);

   while (busy) {

      TickType_t decorrido = xTaskGetTickCount() - inicio;

      if (decorrido >= limite)
         break;

      if (xSemaphoreTake(telegramIdleSem, limite - decorrido) != pdTRUE)
         break;                                // expirou

      // O evento pode ser de uma liberação anterior: reconfere o estado real.
      xSemaphoreTake(serviceMutex, portMAX_DELAY);
      busy = telegramBusy;
      xSemaphoreGive(serviceMutex);
   }

   if (busy) {
      DBG("closeServiceWindow(): TIMEOUT. Ciclo será adiado sem tocar no rádio.\n");
      return false;                            // invariante preservado
   }

   return true;
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

void initConfigMutex() {

   configMutex = xSemaphoreCreateMutex();
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

void lockConfig() {
   if (configMutex != nullptr)
      xSemaphoreTake(configMutex, portMAX_DELAY);
}

void unlockConfig() {
   if (configMutex != nullptr)
      xSemaphoreGive(configMutex);
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

   bool enfileirada = xQueueSend(telegramMessageQueue, &message, 0) == pdPASS;

   if (!enfileirada)
      DBG("telegramMessageQueue cheia: mensagem descartada\n");

   return enfileirada;
}

bool getTelegramMessage(TelegramMessage &message) {
   if (telegramMessageQueue == nullptr)
      return false;

   // Peek: NÃO remove o item. A remoção só ocorre após o envio confirmado
   // (ou após esgotar as tentativas), para permitir nova tentativa na
   // próxima rodada.
   if (xQueuePeek(telegramMessageQueue, &message, 0) != pdPASS)
      return false;

   DBG("Mensagem vista na fila do Telegram: %s\n", message.text);

   return true;
}

// Remove a mensagem do topo da fila: usar após envio bem-sucedido ou após
// esgotar as tentativas de envio.
static bool discardTelegramMessage(TelegramMessage &reuse) {
   if (telegramMessageQueue == nullptr)
      return false;

   bool removida = xQueueReceive(telegramMessageQueue, &reuse, 0) == pdPASS;

   if (removida)
      telegramMessageAttempts = 0;

   return removida;
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