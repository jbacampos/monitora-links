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

   WiFi.setAutoReconnect(false);

   if (!initFS()) {
      DBG("ERRO LITTLEFS. Tentando formatar o sistema de arquivos...\n");
      if (!LittleFS.format()) {
         DBG("ERRO: falha ao formatar LittleFS. Reiniciando...\n");
         while (true)
            delay(1000);
      }

      if (!initFS()) {
         DBG("ERRO LITTLEFS incluso após formatar.\n");
         while (true)
            delay(1000);
      }
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

   // wifiFailCycles conta ciclos de falha de Wi-Fi consecutivos para filtrar
   // falhas transitórias; é um contador de SESSÃO. Como RuntimeData é gravado
   // inteiro por outros módulos (notificações, ack do Telegram, ...), o valor
   // lido do arquivo pode ser de uma sequência de falhas anterior. Zerar aqui
   // garante que a graça de WIFI_FAIL_CYCLES valha por inteiro depois de cada
   // reinicialização.
   for (uint8_t i = 0; i < MAX_LINKS; i++)
      gRuntime.links[i].wifiFailCycles = 0;

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
   // Persiste o motivo DESTE boot para que onBoot() o reporte corretamente.
   // Não sobrescreve os marcadores gravados por doReboot()/doOta() antes do
   // restart (esses chegam aqui como ESP_RST_SW e devem ser preservados).
   if (gRuntime.bootReason != BOOT_AFTER_REBOOT_COMMAND &&
       gRuntime.bootReason != BOOT_AFTER_OTA) {
      gRuntime.bootReason = br;
   }
   gRuntime.saveCount++;
   saveStorage(FILE_RUNTIME, gRuntime);
}

//=============================================================================
// Diagnóstico
//=============================================================================

BootReason getBootReason() {

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


}
