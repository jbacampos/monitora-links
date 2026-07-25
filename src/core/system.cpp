#include "core/system.h"

#include <Arduino.h>

#include "config/config.h"
#include "config/platform.h"
#include "core/storage.h"
#include "core/types.h"

//=============================================================================
// Inicialização
//=============================================================================

uint32_t gCycleCount = 0;

void initSystem() {
   WiFi.persistent(false);

#ifdef ESP8266
   WiFi.setAutoConnect(false);
#endif

   WiFi.setAutoReconnect(false);

   if (!initFS()) {
      DBG("ERRO LITTLEFS\n");
      while (true)
         delay(1000);
   }

   if (!loadStorage(FILE_CONFIG, gConfig, CONFIG_MAGIC, CONFIG_VERSION)) {
      createDefaultConfig();
   }

   if (!loadStorage(FILE_RUNTIME, gRuntime, RUNTIME_MAGIC, RUNTIME_VERSION)) {
      createDefaultRuntime();
   }

   if (!loadStorage(FILE_EVENTS, gEvents, EVENTS_MAGIC, EVENTS_VERSION)) {
      createDefaultEvents();
   }


   BootReason br = getBootReason();

   switch (br) {
   case BOOT_DEEPSLEEP:
      gCycleCount++;
      break;

   case BOOT_WATCHDOG:
      gRuntime.bootCount++;
      gRuntime.watchdogCount++;
      break;

   default:
      gRuntime.bootCount++;
      break;
   }

   saveStorage(FILE_RUNTIME, gRuntime);
}

//=============================================================================
// Diagnóstico
//=============================================================================

BootReason getBootReason() {

#ifdef ESP8266
   rst_info *info = ESP.getResetInfoPtr();
   switch (info->reason) {
      case REASON_DEFAULT_RST:
         return BOOT_POWERON;
      case REASON_DEEP_SLEEP_AWAKE:
         return BOOT_DEEPSLEEP;
      case REASON_WDT_RST:
      case REASON_SOFT_WDT_RST:
         return BOOT_WATCHDOG;
      case REASON_SOFT_RESTART:
         return BOOT_SOFTWARE;
      default:
         return BOOT_UNKNOWN;
   }

#elif defined(ESP32)
   #include <esp_system.h>
   switch (esp_reset_reason()) {
      case ESP_RST_POWERON:
         return BOOT_POWERON;
      case ESP_RST_DEEPSLEEP:
         return BOOT_DEEPSLEEP;
      case ESP_RST_TASK_WDT:
      case ESP_RST_INT_WDT:
      case ESP_RST_WDT:
         return BOOT_WATCHDOG;
      case ESP_RST_SW:
         return BOOT_SOFTWARE;
      default:
         return BOOT_UNKNOWN;
   }

#else

   return BOOT_UNKNOWN;

#endif

}




//=============================================================================
// Energia
//=============================================================================

void goToSleep(uint32_t segundos) {
// Reservada para possível uso no futuro
#if ENABLE_SLEEP
   DBG("Entrando em Deep Sleep\n");
   ESP.deepSleep(segundos * 1000000ULL);
#endif
}