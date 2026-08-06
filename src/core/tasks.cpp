#include "config/config.h"
#include "core/tasks.h"
#include "hardware/led.h" 

#ifdef ESP32

#include <Arduino.h>
#include <network/telegram.h>
#include <network/wifi_manager.h>
#include <core/storage.h>

// tasks.cpp

static TaskHandle_t ledTaskHandle = nullptr;
static TaskHandle_t telegramTaskHandle = nullptr;

static volatile bool serviceWindowOpen = false;
static void telegramTask(void* parameter);

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

static void ledTask(void* parameter) {

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

   BaseType_t result = xTaskCreatePinnedToCore(
      ledTask,
      "LedTask",
      4096,
      nullptr,
      1,
      &ledTaskHandle,
      0
   );

   if (result == pdPASS)
      DBG("Task do LED criada com sucesso\n");
   else
      DBG("ERRO ao criar task do LED\n");
}

//=============================================================================
// Telegram
//=============================================================================

void initTelegramTask() {

   BaseType_t result = xTaskCreatePinnedToCore(
      telegramTask,
      "TelegramTask",
      8192,
      nullptr,
      1,
      &telegramTaskHandle,
      0
   );

   if (result == pdPASS)
      DBG("Task do Telegram criada com sucesso\n");
   else
      DBG("ERRO ao criar task do Telegram\n");
}

static void telegramTask(void* parameter) {

   DBG("Task do Telegram iniciada no core %d\n", xPortGetCoreID());

   TelegramUpdate upd;
   uint32_t proximoGetUpdates = 0;

   for (;;) {

      if (!serviceWindowOpen || WiFi.status() != WL_CONNECTED) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      if (millis() < proximoGetUpdates) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      proximoGetUpdates = millis() + TELEGRAM_GET_UPDATES_INTERVAL;

      if (!telegramGetUpdates(&upd)) {
         vTaskDelay(pdMS_TO_TICKS(50));
         continue;
      }

      DBG("Update recebido = %u - %s\n",
          upd.updateId,
          upd.text.c_str());

      gRuntime.telegramUpdateId = upd.updateId;
      gRuntime.saveCount++;
      saveStorage(FILE_RUNTIME, gRuntime);

      if (!isAuthorizedChat(upd.chatId)) {
         telegramSendMessage(
            "⛔ Chat não autorizado.\nUse o MonitLinks"
         );
         continue;
      }

      if (upd.text.isEmpty())
         continue;

      DBG("upd.text = %s\n", upd.text.c_str());

      CommandResult cmdResult =
         telegramProcessCommand(upd.text);

      if (!cmdResult.message.isEmpty()) {

         DBG("Vai enviar resposta ao comando...\n");

         if (!telegramSendMessage(cmdResult.message)) {
            DBG("Falha ao enviar resposta ao comando\n");
            continue;
         }
      }

      if (cmdResult.deferredFunction != nullptr) {
         DBG("Executando deferredFunction\n");
         cmdResult.deferredFunction();
      }
   }
}

void openServiceWindow() {
   serviceWindowOpen = true;
}

void closeServiceWindow() {
   serviceWindowOpen = false;
}


#endif