#include "config/config.h"
#include "config/platform.h"
#include "core/storage.h"
#include "core/system.h"
#include "core/tasks.h"
#include "core/types.h"
#include "hardware/led.h"
#include "network/monitor.h"
#include "network/notify.h"
#include "network/ntp.h"
#include "network/telegram.h"
#include "network/wifi_manager.h"
#include "profile/profile.h"
#include "reports/eventlog.h"
#include "reports/events.h"
#include "reports/reports.h"
#include "reports/stats.h"

constexpr uint8_t MAX_UPDATES_PER_CYCLE = 10;

void setup() {

   Serial.begin(115200);

   ledInit();
   ledStartup();

#ifdef ESP32
   initLedTask();     // começa imediatamente a sequência visual
   initNotificationMutex();
   initRuntimeMutex();
   WiFi.onEvent(onWiFiEvent);
   initTelegramTask();

   DBG("setup() executando no core %d\n", xPortGetCoreID());
#endif


   String msg = "\n==========================\n";
   msg += "Sistema iniciado\n";
   msg += "==========================\n";

   DBG("%s", msg.c_str());

   gPerfil = detectProfile();

   if (gPerfil == nullptr) {
      DBG("Local desconhecido.\n");
      while (true)
         delay(1000);
   }

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      if (connectWifi(gPerfil->links[i].ssid, gPerfil->links[i].senha)) {
         syncClock();
         break;
      }

      DBG("Falha na conexão Wi-Fi inicial para sincronizar relógio. "
          "Tentando novamente em 5 segundos...\n");

      delay(5000);
   }

   initSystem();
   ledSystemReady();

#ifdef ESP32
   DBG("setup() executando no core %d\n", xPortGetCoreID());
#endif

   onBoot();

   DBG("\nPerfil selecionado: %s\n", gPerfil->nome);
   DBG("Número de links: %u\n", gPerfil->numLinks);

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      DBG("Link %u: %s\n", i + 1, gPerfil->links[i].nome);
   }

//    // ******************************************
//    // Para listar detalhes do sistema no início:
//    // ******************************************
//    // DBG("%s", buildSystemSummary().c_str());
//    // DBG("%s", buildStatus().c_str());
//    // DBG("%s", buildLog(10).c_str());
//    // DBG("%s", buildStatistics(5).c_str());

   // ******************************************
   // Somente para apagar um arquivo:
   // ******************************************
   // resetConfig();
   // resetRuntime();
   // resetEvents();

}

void loop() {

    // A conexão mantida entre ciclos pertenceu à janela de serviços.
   // Encerra-a antes de iniciar os testes.
#ifdef ESP32
   closeServiceWindow();
#endif   
   disconnectWifi();
   ledBeginCycle();
   int16_t rssi;
   LinkStatus status[gPerfil->numLinks];
   // TelegramUpdate upd;
   // bool telegramChecked = false;
   // uint32_t proximoGetUpdates = millis();

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
#ifdef ESP8266
      ledUpdate();
#endif
      DBG("\n=== %s ===\n", gPerfil->links[i].nome);

      uint8_t retries = (gRuntime.links[i].status == LINK_ONLINE) ? LINK_TEST_RETRIES : 1;
      status[i] = testConnection(gPerfil->links[i].ssid, gPerfil->links[i].senha, &rssi, retries);
      DBG("Status do link %s: %s. LINK_WIFI_FAIL = %d\n", gPerfil->links[i].nome, linkStatusDescription(status[i]), LINK_WIFI_FAIL);
      if (status[i] == LINK_WIFI_FAIL) {
         gRuntime.links[i].wifiFailCycles++;
         
         DBG("LINK_WIFI_FAIL no ciclo %d. Máximo de ciclos ignorados = %d\n", gRuntime.links[i].wifiFailCycles, WIFI_FAIL_CYCLES);

         if (gRuntime.links[i].wifiFailCycles < WIFI_FAIL_CYCLES) {
            DBG("Falha Wi-Fi %u/%u - ignorada neste ciclo\n", gRuntime.links[i].wifiFailCycles, WIFI_FAIL_CYCLES);
            continue;   // não chama processLink()
         }
      }
      else {
         gRuntime.links[i].wifiFailCycles = 0;
      }

      processLinkState(&gRuntime.links[i], i, status[i], rssi);

      // Verifica se existe um link que caiu durante
      // o horário de silêncio e gera a notificação
      // quando sair dele:
      checkNotificationPolicy();

      disconnectWifi();
   }

   uint8_t online = 0;

   for (uint8_t i = 0; i < gPerfil->numLinks; i++)
      if (status[i] == LINK_ONLINE)
         online++;

   // DBG("\nLinks online: %u/%u\n", online, gPerfil->numLinks);

   bool serviceConnection = false;

   if (online > 0)
      serviceConnection = connectToOnlineLink(status);

   if (serviceConnection)
      openServiceWindow();

   LedStatus ledStatus;
   DBG("\nCiclo %lu concluído. Links on-line: %u/%u\n", gCycleCount + 1, online, gPerfil->numLinks);
   if (online == gPerfil->numLinks)
      ledStatus = LED_ALL_UP;
   else if (online == 0)
      ledStatus = LED_ALL_DOWN;
   else
      ledStatus = LED_PARTIAL_DOWN;

   gCycleCount++;

   ledEndCycle(ledStatus);

   // Mantém o LED exibindo o estado consolidado do sistema
   // por alguns segundos antes de iniciar um novo ciclo.
   // Isso facilita a inspeção visual do monitor.
   // DBG("\nledStatus: %s\n", ledStatus == LED_ALL_UP ? "TODOS OS LINKS ON-LINE" : (ledStatus == LED_ALL_DOWN ? "TODOS OS LINKS OFF-LINE" : "ALGUNS LINKS OFF-LINE"));
   DBG("\nVai dormir por %u milissegundos...\n", LED_STATUS_HOLD_MS);
   delay(LED_STATUS_HOLD_MS);

}

