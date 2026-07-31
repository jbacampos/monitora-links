#include "config/config.h"
#include "config/platform.h"
#include "core/storage.h"
#include "core/system.h"
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
constexpr uint32_t TELEGRAM_GET_UPDATES_INTERVAL = 5000; // 1 segundo

void setup() {

   Serial.begin(115200);
   delay(6000);
   
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
      DBG("Falha na conexão Wi-Fi inicial para sincronizar relógio. Tentando novamente em 5 segundos...\n");
      delay(5000);
   }

   initSystem();
   ledsInit();
   
   String msg = "\n==========================\n";
   msg += "Sistema iniciado\n";
   msg += "==========================\n";

   DBG(msg.c_str());

   onBoot();

   DBG("\nPerfil selecionado: %s\n", gPerfil->nome);
   DBG("Número de links: %u\n", gPerfil->numLinks);
   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      DBG("Link %u: %s\n", i + 1, gPerfil->links[i].nome);
   }

   DBG("%s", buildSystemSummary().c_str());
   DBG("%s", buildStatus().c_str());
   DBG("%s", buildLog(10).c_str());
   DBG("%s", buildStatistics(5).c_str());

   // Somente para apagar um arquivo:
   // resetConfig();
   // resetRuntime();
   // resetEvents();

}

void loop() {

   ledsBeginCycle();
   int16_t rssi;
   LinkStatus status[gPerfil->numLinks];
   TelegramUpdate upd;
   bool telegramChecked = false;
   uint32_t proximoGetUpdates = millis();

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      ledsUpdate();
      DBG("\n=== %s ===\n", gPerfil->links[i].nome);

      uint8_t retries =  (gRuntime.links[i].status == LINK_ONLINE) ? LINK_TEST_RETRIES : 1;
      status[i] = testConnection(gPerfil->links[i].ssid, gPerfil->links[i].senha, &rssi, retries);

      processLink(&gRuntime.links[i], i, status[i], rssi);

      // Verifica se existe um link que caiu durante
      // o horário de silêncio e gera a notificação
      // quando sair dele:
      checkNotificationPolicy();

      if (hasPendingNotifications())
         sendPendingNotifications();

      uint32_t t0 = millis();
      uint32_t t1 = millis();
      if (!telegramChecked && status[i] == LINK_ONLINE) {
         // DBG("telegramUpdateId = %lu\n", gRuntime.telegramUpdateId);
         // DBG("Consultando Telegram...\n");
         for (uint8_t i = 0; i < MAX_UPDATES_PER_CYCLE; i++) {

            if (millis() < proximoGetUpdates) {
               break;
            }
            proximoGetUpdates = millis() + TELEGRAM_GET_UPDATES_INTERVAL;

            DBG("Vai chamar telegramGetUpdates...\n");
            t0 = millis();
            if (!telegramGetUpdates(&upd)) {
               DBG("Chamou telegramGetUpdates, resultado = false, tempo = %lu "
                   "ms\n",
                   millis() - t0);
               break;
            }
            telegramChecked = true;
            DBG("Chamou telegramGetUpdates, resultado = true, tempo = %lu ms\n",
                millis() - t0);
            DBG("update recebido = %u\n", upd.updateId);

            if (!isAuthorizedChat(upd.chatId)) {
               telegramSendMessage("⛔ Chat não autorizado.\nUse o MonitLinks");
               continue;
            }
            if (upd.text.isEmpty()) {
               continue;
            }
            DBG("upd.text = %s\n", upd.text.c_str());

            t0 = millis();
            CommandResult cmdResult = telegramProcessCommand(upd.text);
            DBG("Chamou telegramProcessCommand, resposta = %s, tempo = %lu "
                "ms\n",
                cmdResult.message.c_str(),
                millis() - t0);
            if (!cmdResult.message.isEmpty()) {
               t0 = millis();
               DBG("Vai chamar telegramSendMessage...\n");
               if (!telegramSendMessage(cmdResult.message)) {
                  DBG("Chamou telegramSendMessage, resultado = false, tempo = "
                      "%lu ms\n",
                      millis() - t0);
                  break;
               }
               DBG("Chamou telegramSendMessage, resultado = true, tempo = %lu "
                   "ms\n",
                   millis() - t0);
            }
            DBG("Novo updateId = %lu\n", upd.updateId);
            DBG("updateId anterior = %lu\n", gRuntime.telegramUpdateId);
            gRuntime.telegramUpdateId = upd.updateId;
            saveStorage(FILE_RUNTIME, gRuntime);

            if (cmdResult.deferredFunction != nullptr) {
               DBG("Tem deferredFunction, vai executá-la:\n");
               cmdResult.deferredFunction();
            }
         }
      }

      disconnectWifi();
   }

   uint8_t online = 0;

   for (uint8_t i = 0; i < gPerfil->numLinks; i++)
      if (status[i] == LINK_ONLINE)
         online++;

   DBG("\nLinks online: %u/%u\n", online, gPerfil->numLinks);

   LedStatus ledStatus;
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
   DBG("\nledStatus: %s\n",
       ledStatus == LED_ALL_UP
           ? "TODOS OS LINKS ON-LINE"
           : (ledStatus == LED_ALL_DOWN ? "TODOS OS LINKS OFF-LINE"
                                        : "ALGUNS LINKS OFF-LINE"));
   DBG("\nVai dormir por %u milissegundos...\n", LED_STATUS_HOLD_MS);
   delay(LED_STATUS_HOLD_MS);
   goToSleep(300); // Desabilitado enquanto não for necessário
}
