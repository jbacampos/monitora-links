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

   String msg = "\n==========================\n";
   msg += "Sistema iniciado\n";
   msg += "==========================\n";

   DBG("%s", msg.c_str());

   ledInit();
   ledStartup();

   initLedTask();     // começa imediatamente a sequência visual
   initRuntimeMutex();
   initEventsMutex();
   WiFi.onEvent(onWiFiEvent);
   initTelegramTask();

   DBG("setup() executando no core %d\n", xPortGetCoreID());


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

   DBG("setup() executando no core %d\n", xPortGetCoreID());

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
   closeServiceWindow();
   disconnectWifi();
   ledBeginCycle();
   int16_t rssi;
   LinkStatus status[gPerfil->numLinks];
   uint8_t wifiFailCycles;


   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      ledUpdate();
      DBG("\n=== %s ===\n", gPerfil->links[i].nome);

      lockRuntime();
      uint8_t retries = (gRuntime.links[i].status == LINK_ONLINE) ? LINK_TEST_RETRIES : 1;
      unlockRuntime();
      
      status[i] = testConnection(gPerfil->links[i].ssid, gPerfil->links[i].senha, &rssi, retries);

      lockRuntime();
      wifiFailCycles = gRuntime.links[i].wifiFailCycles;
      unlockRuntime();

      // DBG("Status do link %s: %s. LINK_WIFI_FAIL = %d\n", gPerfil->links[i].nome, linkStatusDescription(status[i]), LINK_WIFI_FAIL);
      if (status[i] == LINK_WIFI_FAIL) {
         wifiFailCycles++;

         DBG("LINK_WIFI_FAIL no ciclo %d. Máximo de ciclos ignorados = %d\n", wifiFailCycles, WIFI_FAIL_CYCLES);

         if (wifiFailCycles < WIFI_FAIL_CYCLES) {
            DBG("Falha Wi-Fi %u/%u - ignorada neste ciclo\n", gRuntime.links[i].wifiFailCycles, WIFI_FAIL_CYCLES);
            lockRuntime();
            gRuntime.links[i].wifiFailCycles = wifiFailCycles;
            unlockRuntime();
            continue;   // não chama processLink()

         }
      }
      else {
         wifiFailCycles = 0;
      }

      LinkState state;

      lockRuntime();
      state = gRuntime.links[i];
      unlockRuntime();

      processLinkState(&state, i, status[i], rssi);

      lockRuntime();
      gRuntime.links[i] = state;
      unlockRuntime();

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

   //************************************************************************
   // Ver se não dá apra deixar a última conexão aberta, para evitar o 
   // overhead de reconectar para enviar notificações e atualizar o Telegram.
   //************************************************************************

   bool serviceConnection = false;

   if (online > 0)
      serviceConnection = connectToOnlineLink(status);

   if (serviceConnection)
      openServiceWindow();

   MonitorCommand command = {};

   while (getMonitorCommand(command)) {
      if (strcmp(command.text, "/s") == 0) {
         CommandResult result = cmdStatus("");
         DBG("Comando retirado da fila: %s\n", command.text);
         queueTelegramMessage(result.message.c_str());
      } else if (strncmp(command.text, "/e", 2) == 0 ||
         strncmp(command.text, "/stats", 6) == 0) {
         String text = command.text;
         String args;
         int p = text.indexOf(' ');
         if (p >= 0) {
            args = text.substring(p + 1);
            args.trim();
         }
         CommandResult result = cmdStats(args);
         queueTelegramMessage(result.message.c_str());
      } else if (strncmp(command.text, "/l", 2) == 0 ||
         strncmp(command.text, "/log", 4) == 0) {
         String text = command.text;
         String args;
         int p = text.indexOf(' ');
         if (p >= 0) {
            args = text.substring(p + 1);
            args.trim();
         }
         CommandResult result = cmdLog(args);
         queueTelegramMessage(result.message.c_str());
      } else if (strncmp(command.text, "/n", 2) == 0 ||
         strncmp(command.text, "/notify", 7) == 0) {

         String text = command.text;
         String args;

         int p = text.indexOf(' ');

         if (p >= 0) {
            args = text.substring(p + 1);
            args.trim();
         }

         CommandResult result = cmdNotify(args);

         queueTelegramMessage(result.message.c_str());
      } else if (strncmp(command.text, "/q", 2) == 0 ||
         strncmp(command.text, "/quiet", 6) == 0) {

         String text = command.text;
         String args;

         int p = text.indexOf(' ');

         if (p >= 0) {
            args = text.substring(p + 1);
            args.trim();
         }

         CommandResult result = cmdQuiet(args);

         queueTelegramMessage(result.message.c_str());
      }
   }

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

