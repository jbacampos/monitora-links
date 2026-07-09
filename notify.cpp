#include "notify.h"

#include <ESP8266WiFi.h>

#include "config.h"
#include "ntp.h"
#include "storage.h"
#include "telegram.h"

//=============================================================================
// Auxiliares
//=============================================================================

static String buildMessage(const PendingNotification &n) {

   String inicioTxt = (n.inicio != 0) ? formatDateTime(n.inicio, DATETIME_SHORT)
                                      : "desconhecido";
   String duracaoTxt =
       (n.inicio != 0) ? formatDuration(n.duracao) : "desconhecida";
   String msg;

   if (n.tipo == NOTIFY_DOWN) {
      msg += LINKS[n.link].emojiOffline;
      msg += " ";
      msg += LINKS[n.link].nome;
      msg += " OFFLINE\n\n";

      msg += "Motivo :   ";
      msg += linkStatusDescription(n.motivo);
      msg += "\n";

      msg += "Inicio    :   ";
      msg += inicioTxt;
      msg += "\n";

      msg += "Evento :    #";
      msg += String(n.evento);

   } else {
      msg += LINKS[n.link].emojiOnline;
      msg += " ";
      msg += LINKS[n.link].nome;
      msg += " ONLINE\n\n";

      msg += "Inicio      :   ";
      msg += inicioTxt;
      msg += "\n";

      msg += "Retorno :   ";
      msg += formatDateTime(n.fim, DATETIME_SHORT);
      msg += "\n";

      msg += "Duracao :   ";
      msg += duracaoTxt;
      msg += "\n";

      msg += "Motivo :   ";
      msg += linkStatusDescription(n.motivo);
      msg += "\n";

      msg += "RSSI       :    ";
      msg += String(n.rssi);
      msg += " dBm\n";

      msg += "Evento   :     #";
      msg += String(n.evento);
   }
   return msg;
}

//=============================================================================
// Fila de notificações
//=============================================================================

void queueNotification(const PendingNotification &n) {
   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      if (!gState.pendingNotifications[i].pending) {
         gState.pendingNotifications[i] = n;
         gState.pendingNotifications[i].pending = true;
         saveState(&gState);
         return;
      }
   }

   DBG("Fila de notificações cheia.\n");
}

//=============================================================================
// Consulta
//=============================================================================

bool hasPendingNotifications() {

   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      if (gState.pendingNotifications[i].pending)
         return true;
   }

   return false;
}

uint8_t getPendingNotificationCount() {

   uint8_t totPends = 0;
   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      if (gState.pendingNotifications[i].pending)
         totPends++;
   }

   return totPends;
}

//=============================================================================
// Envio
//=============================================================================

void clearPendingNotifications() {
   bool mudou = false;

   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      if (gState.pendingNotifications[i].pending) {
         gState.pendingNotifications[i].pending = false;
         mudou = true;
      }
   }

   if (mudou)
      saveState(&gState);
}

void sendPendingNotifications() {
   if (WiFi.status() != WL_CONNECTED)
      return;

   if (!gState.notification.enabled) {
      clearPendingNotifications();
      return;
   }
   if (inQuietHours())
      return;

   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      PendingNotification &n = gState.pendingNotifications[i];

      if (!n.pending)
         continue;

      String msg = buildMessage(n);

      if (telegramSendMessage(msg)) {
         n.pending = false;
         saveState(&gState);
      } else {
         DBG("Falha ao enviar notificacao #%lu\n",
             gState.pendingNotifications[i].evento);
         break;
      }
   }
}

//=============================================================================
// Política de notificações
//=============================================================================

static bool wasInQuietHours = false;

static uint16_t minutesOfDay(time_t t) {
   t += GMT_OFFSET_SEC;
   struct tm tm;
   gmtime_r(&t, &tm);
   return tm.tm_hour * 60 + tm.tm_min;
}

bool quietHoursEnabled() { return gState.notification.quietEnabled; }

bool inQuietHours(time_t t) {

   if (!gState.notification.quietEnabled)
      return false;

   if (t == 0) {
      if (!clockIsValid())
         return false;
      t = now();
   }

   uint16_t min = minutesOfDay(t);

   uint16_t ini = gState.notification.quietStart;
   uint16_t fim = gState.notification.quietEnd;

   if (ini < fim)
      return (min >= ini && min < fim);

   return (min >= ini || min < fim);
}

void checkNotificationPolicy() {

   if (!clockIsValid())
      return;

   bool quietNow = inQuietHours();

   // Nenhuma transição.
   if (quietNow == wasInQuietHours)
      return;

   wasInQuietHours = quietNow;

   // Entrou no horário de silêncio.
   if (quietNow) {
      saveState(&gState);
      return;
   }

   // Acabou de sair do horário de silêncio.

   for (uint8_t i = 0; i < NUM_LINKS; i++) {

      LinkState *ls = &gState.links[i];

      if (ls->status == LINK_ONLINE)
         continue;

      if (ls->inicioFalha == 0)
         continue;

      if (ls->downNotificationSent)
         continue;

      // A falha precisa ter começado durante o período de silêncio.
      if (!inQuietHours(ls->inicioFalha))
         continue;

      PendingNotification n = {};

      n.pending = true;
      n.link = i;
      n.tipo = NOTIFY_DOWN;
      n.motivo = ls->status;
      n.evento = ls->eventoAtual;
      n.inicio = ls->inicioFalha;
      n.rssi = ls->ultimoRSSI;

      queueNotification(n);

      ls->downNotificationSent = true;
   }

   saveState(&gState);
}
