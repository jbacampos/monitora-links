#include "system.h"

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <user_interface.h>

#include "config.h"
#include "storage.h"
#include "types.h"

//=============================================================================
// Inicialização
//=============================================================================

void initSystem() {
   WiFi.persistent(false);
   WiFi.setAutoConnect(false);
   WiFi.setAutoReconnect(false);

   if (!initFS()) {
      DBG("ERRO LITTLEFS\n");

      while (true)
         delay(1000);
   }

   if (!loadState(&gState)) {
      createDefaultState();
   }

   BootReason br = getBootReason();

   switch (br) {
   case BOOT_DEEPSLEEP:
      gState.cicloCount++;
      break;

   case BOOT_WATCHDOG:
      gState.bootCount++;
      gState.watchdogCount++;
      break;

   default:
      gState.bootCount++;
      break;
   }

   saveState(&gState);
}

//=============================================================================
// Diagnóstico
//=============================================================================

BootReason getBootReason() {
   rst_info *rst = ESP.getResetInfoPtr();

   switch (rst->reason) {
   case REASON_DEFAULT_RST:
      return BOOT_POWERON;

   case REASON_EXT_SYS_RST:
      return BOOT_EXTERNAL;

   case REASON_WDT_RST:
   case REASON_SOFT_WDT_RST:
      return BOOT_WATCHDOG;

   case REASON_DEEP_SLEEP_AWAKE:
      return BOOT_DEEPSLEEP;

   default:
      return BOOT_UNKNOWN;
   }
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