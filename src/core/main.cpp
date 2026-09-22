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

static void processMonitorCommand(const MonitorCommand &command);

static String commandArgs(const MonitorCommand &command)
{
   String text = command.text;
   int p = text.indexOf(' ');
   if (p < 0)
      return "";
   String args = text.substring(p + 1);
   args.trim();
   return args;
}

static void processPendingMonitorCommands() {
   MonitorCommand command = {};

   while (getMonitorCommand(command))
      processMonitorCommand(command);
}

static void processMonitorCommand(const MonitorCommand &command) {

   if (strcmp(command.text, "/s") == 0 || strcmp(command.text, "/status") == 0) {
      CommandResult result = cmdStatus("");
      queueTelegramMessage(result.message.c_str());

   } else if (strncmp(command.text, "/led", 4) == 0) {

      CommandResult result = cmdLed(commandArgs(command));
      queueTelegramMessage(result.message.c_str());

   } else if (strncmp(command.text, "/e", 2) == 0 || strncmp(command.text, "/stats", 6) == 0) {
      CommandResult result = cmdStats(commandArgs(command));
      queueTelegramMessage(result.message.c_str());

   } else if (strncmp(command.text, "/l", 2) == 0 || strncmp(command.text, "/log", 4) == 0) {
      CommandResult result = cmdLog(commandArgs(command));
      queueTelegramMessage(result.message.c_str());

   } else if (strncmp(command.text, "/n", 2) == 0 || strncmp(command.text, "/notify", 7) == 0) {

      CommandResult result = cmdNotify(commandArgs(command));
      queueTelegramMessage(result.message.c_str());

   } else if (strncmp(command.text, "/q", 2) == 0 || strncmp(command.text, "/quiet", 6) == 0) {

      CommandResult result = cmdQuiet(commandArgs(command));
      queueTelegramMessage(result.message.c_str());

   } else if (strcmp(command.text, "/h") == 0 || strcmp(command.text, "/help") == 0) {
      CommandResult result = cmdHelp("");
      queueTelegramMessage(result.message.c_str());

   } else if (strcmp(command.text, "/reboot") == 0) {
      queueTelegramMessage("🔄 Reiniciando o monitor...",TELEGRAM_ACTION_REBOOT);

   } else if (strcmp(command.text, "/ota") == 0) {
      queueTelegramMessage("🔄 Atualizando o firmware...",TELEGRAM_ACTION_OTA);
   }

}

void setup() {

   Serial.begin(115200);

   DBG("\n==========================\n"
      "Sistema iniciado\n"
      "==========================\n");

   ledInit();
   ledStartup();

   initLedTask();     // começa imediatamente a sequência visual
   initRuntimeMutex();
   initConfigMutex();
   initEventsMutex();
   WiFi.onEvent(onWiFiEvent);

   DBG("setup() executando no core %d\n", xPortGetCoreID());


   gPerfil = detectProfile();

   if (gPerfil == nullptr) {
      DBG("Local desconhecido.\n");
      while (true)
         delay(1000);
   }

   bool clockSynced = false;
   for (uint8_t i = 0; i < gPerfil->numLinks && !clockSynced; i++) {
      if (connectWifi(gPerfil->links[i].ssid, gPerfil->links[i].senha)) {
         clockSynced = syncClock();
         if (!clockSynced)
            disconnectWifi();
         continue;
      }

      DBG("Falha na conexão Wi-Fi inicial para sincronizar relógio. "
          "Tentando novamente em 5 segundos...\n");

      delay(5000);
   }

   for (uint8_t tentativa = 0; !clockSynced && tentativa < 3; tentativa++) {
      DBG("Relógio ainda inválido. Nova tentativa de sincronização (%u/3)...\n", tentativa + 1);
      if (WiFi.status() != WL_CONNECTED) {
         if (!connectWifi(gPerfil->links[0].ssid, gPerfil->links[0].senha))
            continue;
      }
      clockSynced = syncClock();
      if (!clockSynced)
         delay(1000);
   }

   if (!clockSynced)
      DBG("AVISO: não foi possível sincronizar o relógio durante o boot.\n");

   initSystem();
   ledSystemReady();

   onBoot();
   initTelegramTask();

   DBG("\nPerfil selecionado: %s\n", gPerfil->nome);
   DBG("Número de links: %u\n", gPerfil->numLinks);

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      DBG("Link %u: %s\n", i + 1, gPerfil->links[i].nome);
   }

   // ******************************************
   // Somente para apagar um arquivo:
   // ******************************************
   // resetConfig();
   // resetRuntime();
   // resetEvents();

}

void loop() {

   // Processa comandos recebidos antes de iniciar uma nova rodada de testes.
   // Assim, uma resposta não fica atrás de todo o ciclo de monitoramento.
   processPendingMonitorCommands();

   // A conexão mantida entre ciclos pertenceu à janela de serviços.
   // Encerra-a antes de iniciar os testes.
   closeServiceWindow();
   disconnectWifi();
   ledBeginCycle();
   int16_t rssi;
   LinkStatus status[gPerfil->numLinks];
   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      ledUpdate();
      DBG("\n=== %s ===\n", gPerfil->links[i].nome);

      LinkState currentState;
      lockRuntime();
      currentState = gRuntime.links[i];
      unlockRuntime();

      uint8_t retries = (currentState.status == LINK_ONLINE) ? LINK_TEST_RETRIES : 1;
      
      status[i] = testConnection(gPerfil->links[i].ssid, gPerfil->links[i].senha, &rssi, retries);

      LinkState state = currentState;

      // DBG("Status do link %s: %s. LINK_WIFI_FAIL = %d\n", gPerfil->links[i].nome, linkStatusDescription(status[i]), LINK_WIFI_FAIL);
      if (status[i] == LINK_WIFI_FAIL) {
         if (state.wifiFailCycles < UINT8_MAX)
            state.wifiFailCycles++;
         if (state.inicioFalha == 0) {
            state.inicioFalha = now();
         }

         uint32_t failElapsedSec = (uint32_t)(now() - state.inicioFalha);

         DBG("LINK_WIFI_FAIL no ciclo %d. Falha persistente em %lu/%lu s. Máximo de ciclos ignorados = %d\n",
             state.wifiFailCycles, (unsigned long)failElapsedSec, (unsigned long)WIFI_FAIL_GRACE_SEC, WIFI_FAIL_CYCLES);

         if (failElapsedSec < WIFI_FAIL_GRACE_SEC || state.wifiFailCycles < WIFI_FAIL_CYCLES) {
            DBG("Falha Wi-Fi transitória %u/%u - ignorada neste ciclo (grace %lu s)\n",
                state.wifiFailCycles, WIFI_FAIL_CYCLES, (unsigned long)WIFI_FAIL_GRACE_SEC);
            lockRuntime();
            gRuntime.links[i] = state;
            unlockRuntime();
            continue;   // não chama processLink()
         }

      }
      else {
          state.wifiFailCycles = 0;

         // Falha Wi-Fi ignorada no periodo de graca nao representa queda
         // confirmada; descarte o marco temporario.
         if (state.status == LINK_ONLINE)
            state.inicioFalha = 0;
      }

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

   processPendingMonitorCommands();

   
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
   // Primeiro, faz metade do delay() previsto e testa se há alguma ação pendente do Monitor. Se houver, executa-a imediatamente.
   DBG("\nVai dormir por %u milissegundos...\n", LED_STATUS_HOLD_MS / 2);
   delay(LED_STATUS_HOLD_MS / 2);
   
   MonitorAction action;
   if (getMonitorAction(action)) {
      
      closeServiceWindow();

      switch (action) {
         case ACTION_REBOOT:
            DBG("Executando ACTION_REBOOT no Monitor.\n");
            doReboot();
            break;
         case ACTION_OTA:
            DBG("Executando ACTION_OTA no Monitor.\n");
            ledEndCycle(LED_OTA);
            doOta();
            break;
         default:
            DBG("Monitor action desconhecido: %d\n", action);
            break;
      }
   }

   // Segunda metade do delay():
   DBG("\nVai dormir por mais %u milissegundos...\n", LED_STATUS_HOLD_MS / 2);
   delay(LED_STATUS_HOLD_MS / 2);

}

