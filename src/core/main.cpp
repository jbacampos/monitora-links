#include "config/config.h"
#include "reports/eventlog.h"
#include "reports/events.h"
#include "hardware/led.h"
#include "network/monitor.h"
#include "network/wifi_manager.h"
#include "network/notify.h"
#include "network/ntp.h"
#include "reports/reports.h"
#include "reports/stats.h"
#include "core/storage.h"
#include "core/system.h"
#include "network/telegram.h"
#include "core/types.h"

constexpr uint8_t MAX_UPDATES_PER_CYCLE = 10;

void setup() {

   Serial.begin(115200);
   delay(6000);
   initSystem();
   ledsInit();
   String msg = "\n==========================\n";
   msg += "Sistema iniciado\n";
   msg += "==========================\n";
   DBG(msg.c_str());
   DBG("%s", buildSystemSummary().c_str());
   DBG("%s", buildStatus().c_str());
   DBG("%s", buildLog(10).c_str());
   DBG("%s", buildStatistics(5).c_str());

   // DBG("\n\n");
   // DBG("sizeof(LinkState) = %u\n", sizeof(LinkState));
   // DBG("sizeof(Event) = %u\n", sizeof(Event));
   // DBG("sizeof(PendingNotification) = %u\n", sizeof(PendingNotification));
   // DBG("sizeof(NotificationSettings) = %u\n", sizeof(NotificationSettings));
   // DBG("sizeof(PersistState) = %u\n", sizeof(PersistState));

   // DBG("sizeof(time_t) = %u\n", sizeof(time_t));
   // DBG("sizeof(bool) = %u\n", sizeof(bool));
   // DBG("sizeof(LinkStatus) = %u\n", sizeof(LinkStatus));
   // DBG("sizeof(NotificationType) = %u\n", sizeof(NotificationType));
   // DBG("sizeof(LinkId) = %u\n", sizeof(LinkId));

   // Somente para apagar todo o histórico:
   // resetState();
}

void loop() {

   ledsBeginCycle();
   int16_t rssi;
   LinkStatus status[NUM_LINKS];
   TelegramUpdate upd;
   bool telegramChecked = false;

   for (uint8_t i = 0; i < NUM_LINKS; i++) {
      ledsUpdate();
      DBG("\n=== %s ===\n", LINKS[i].nome);

      uint8_t retries = (gRuntime.links[i].status == LINK_ONLINE) ? LINK_TEST_RETRIES : 1;
      status[i] = testConnection(LINKS[i].ssid, LINKS[i].senha, &rssi, retries);

      processLink(&gRuntime.links[i], i, status[i], rssi);
       
      // Verifica se existe um link que caiu durante
      // o horário de silêncio e gera a notificação
      // quando sair dele:
      checkNotificationPolicy();

      if (hasPendingNotifications())
         sendPendingNotifications();

      DBG("Vai tratar comandos\n");
      uint32_t t0 = millis();
      if (!telegramChecked && status[i] == LINK_ONLINE) {
         // DBG("telegramUpdateId = %lu\n", gRuntime.telegramUpdateId);
         // DBG("Consultando Telegram...\n");
         for (uint8_t i = 0; i < MAX_UPDATES_PER_CYCLE; i++) {
            if (!telegramGetUpdates(&upd)) {
               break;
            }
            telegramChecked = true;
            DBG("getUpdates: %lu ms\n", millis() - t0);
            DBG("update recebido = %u\n", upd.updateId);

            t0 = millis();

            gRuntime.telegramUpdateId = upd.updateId;
            saveStorage(FILE_RUNTIME, &gRuntime);
            
            DBG("save Runtime: %lu ms\n", millis() - t0);

            if (!isAuthorizedChat(upd.chatId)) {
               telegramSendMessage("⛔ Chat não autorizado.\nUse o MonitLinks");
               continue;
            }
            if (upd.text.isEmpty())
               continue;
            t0 = millis();

            String resposta = telegramProcessCommand(upd.text);
            DBG("processCommand: %lu ms\n", millis() - t0);
            t0 = millis();
            if (!resposta.isEmpty())
               telegramSendMessage(resposta);
            break;
         }
      }
      DBG("sendMessage: %lu ms\n", millis() - t0);
      DBG("Tratou comandos\n");

      disconnectWifi();
   }

   uint8_t online = 0;

   for (uint8_t i = 0; i < NUM_LINKS; i++)
      if (status[i] == LINK_ONLINE)
         online++;

   LedStatus ledStatus;
   if (online == NUM_LINKS)
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
   delay(LED_STATUS_HOLD_MS);
   goToSleep(300); // Desabilitado enquanto não for necessário
}
