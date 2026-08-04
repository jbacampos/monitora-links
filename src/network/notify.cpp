#include "network/notify.h"

#include "config/config.h"
#include "network/ntp.h"
#include "config/platform.h"
#include "core/storage.h"
#include "network/telegram.h"

//=============================================================================
// Auxiliares
//=============================================================================

static String buildMessage(const PendingNotification &n) {

   String inicioTxt = (n.inicio != 0) ? formatDateTime(n.inicio, DATETIME_SHORT) : "desconhecido";
   String duracaoTxt =
       (n.inicio != 0) ? formatDuration(n.duracao) : "desconhecida";
   String msg;

   if (n.tipo == NOTIFY_DOWN) {
      msg += EmojiOffline;
      msg += " ";
      msg += gPerfil->links[n.link].nome;
      msg += " OFFLINE\n\n";

      msg += "Motivo :   ";
      msg += linkStatusDescription(n.motivo);
      msg += "\n";

      msg += "Inicio    :   ";
      msg += inicioTxt;
      msg += "\n";

      msg += "Evento :    #";
      msg += String(n.evento);

   } else if (n.tipo == NOTIFY_UP){
      msg += EmojiOnline;
      msg += " ";
      msg += gPerfil->links[n.link].nome;
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

   } else if (n.tipo == NOTIFY_BOOT) {
      DBG("Montando mensagem de notificacao de reboot\n");
      DBG("Início = %s\n", formatDateTime(n.inicio, DATETIME_SHORT).c_str());
      DBG("Fim = %s\n", formatDateTime(n.fim, DATETIME_SHORT).c_str());
      DBG("Duração = %s\n", formatDuration(n.duracao).c_str());

      msg += EmojiReboot;
      msg += " Monitor reiniciado\n\n";

      if (n.inicio) {
         msg += "Inicio      :  ";
         msg += formatDateTime(n.inicio, DATETIME_SHORT);
         msg += "\n";

         msg += "Fim         :  ";
         msg += formatDateTime(n.fim, DATETIME_SHORT);
         msg += "\n";

         msg += "Duracao :  ";
         msg += duracaoTxt;
         msg += "\n";

      } else {
         msg += "Em           :  ";
         msg += formatDateTime(n.fim, DATETIME_SHORT);
         msg += "\n";

      }

      msg += "Motivo   :  ";
      msg += bootReasonDescription(n.bootReason);
      msg += "\n";

      msg += "Evento    :  #";
      msg += String(n.evento);

   }
   return msg;
}

//=============================================================================
// Fila de notificações
//=============================================================================

void queueNotification(const PendingNotification &n) {
   DBG("Adicionando notificacao #%u na fila\n", n.evento);
   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      if (!gRuntime.pendingNotifications[i].pending) {
         gRuntime.pendingNotifications[i] = n;
         gRuntime.pendingNotifications[i].pending = true;
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
      if (gRuntime.pendingNotifications[i].pending)
         return true;
   }

   return false;
}

uint8_t getPendingNotificationCount() {

   uint8_t totPends = 0;
   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      if (gRuntime.pendingNotifications[i].pending)
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
      if (gRuntime.pendingNotifications[i].pending) {
         gRuntime.pendingNotifications[i].pending = false;
         mudou = true;
      }
   }

   if (mudou) {
      gRuntime.saveCount++;
      saveStorage(FILE_RUNTIME, gRuntime);
   }
}

void sendPendingNotifications() {
   if (WiFi.status() != WL_CONNECTED)
      return;

   if (!gConfig.notification.enabled) {
      clearPendingNotifications();
      return;
   }
   if (inQuietHours())
      return;

   for (uint8_t i = 0; i < MAX_PENDING_NOTIFICATIONS; i++) {
      PendingNotification &n = gRuntime.pendingNotifications[i];

      if (!n.pending)
         continue;

      String msg = buildMessage(n);

      if (telegramSendMessage(msg)) {
         n.pending = false;
         gRuntime.saveCount++;
         saveStorage(FILE_RUNTIME, gRuntime);
      } else {
         DBG("Falha ao enviar notificacao #%u\n",
             gRuntime.pendingNotifications[i].evento);
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

bool quietHoursEnabled() { 
   return gConfig.notification.quietEnabled; 
}

bool inQuietHours(time_t t) {

   if (!gConfig.notification.quietEnabled)
      return false;

   if (t == 0) {
      if (!clockIsValid())
         return false;
      t = now();
   }

   uint16_t min = minutesOfDay(t);

   uint16_t ini = gConfig.notification.quietStart;
   uint16_t fim = gConfig.notification.quietEnd;

   if (ini < fim)
      return (min >= ini && min < fim);

   return (min >= ini || min < fim);
}

void checkNotificationPolicy() {

   bool modificou = false;

   if (!clockIsValid())
      return;

   bool quietNow = inQuietHours();

   // Nenhuma transição.
   if (quietNow == wasInQuietHours)
      return;

   wasInQuietHours = quietNow;

   // Entrou no horário de silêncio.
   if (quietNow)
      return;

   // Acabou de sair do horário de silêncio.

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {

      LinkState *ls = &gRuntime.links[i];

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

      n.link = i;
      n.tipo = NOTIFY_DOWN;
      n.motivo = ls->status;
      n.evento = ls->eventoAtual;
      n.inicio = ls->inicioFalha;
      n.rssi = ls->ultimoRSSI;

      queueNotification(n);
      modificou = true;
      ls->downNotificationSent = true;
   }

   if (modificou) {
      gRuntime.saveCount++;
      saveStorage(FILE_RUNTIME, gRuntime);
   }
   
}
