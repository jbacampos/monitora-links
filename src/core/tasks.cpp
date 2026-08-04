#include "config/config.h"
#include "core/tasks.h"
#include "hardware/led.h" 

#ifdef ESP32

#include <Arduino.h>

// tasks.cpp

static TaskHandle_t ledTaskHandle = nullptr;

static void ledStartupSequence() {

   const LedColor colors[] = {
      LED_RED,
      LED_YELLOW,
      LED_GREEN,
      LED_CYAN,
      LED_BLUE,
      LED_MAGENTA,
      LED_WHITE
   };

// DBG("Iniciando sequência de LEDs\n");

   for (uint8_t n = 0; n < 2  ; n++) {
      for (LedColor color : colors) {
// DBG("Cor startup: %d\n", color);
         setColor(color);
         vTaskDelay(pdMS_TO_TICKS(250));

         setColor(LED_BLANK);
         vTaskDelay(pdMS_TO_TICKS(150));
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


#endif