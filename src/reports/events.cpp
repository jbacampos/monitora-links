
#include "config/config.h"
#include "reports/eventlog.h"
#include "reports/events.h"
#include "network/wifi_manager.h"
#include "network/notify.h"
#include "network/ntp.h"
#include "config/platform.h"
#include "core/storage.h"
#include "core/types.h"

//=============================================================================
// Transição de estado dos links
//=============================================================================

void onLinkDown(LinkState *state, LinkId link, LinkStatus motivo,
                int16_t rssi) {

   gEvents.lastEventId++;
   state->eventoAtual = gEvents.lastEventId;
   if (clockIsValid())
      state->inicioFalha = now();
   else
      state->inicioFalha = 0;
   state->ultimaMudanca = state->inicioFalha;
   state->ultimoRSSI = rssi;
   DBG("\nEvento #%u iniciado\n", state->eventoAtual);
   DBG("Operadora : %s\n", gPerfil->links[link].nome);
   DBG("Motivo    : %s\n", linkStatusDescription(motivo));

   if (quietHoursEnabled() && inQuietHours()) {
      DBG("\n*** Horário de silêncio: notificação não gerada\n");
      state->downNotificationSent = false;
   } else {
      PendingNotification n = {};
      n.link = link;
      n.tipo = NOTIFY_DOWN;
      n.motivo = motivo;
      n.evento = state->eventoAtual;
      n.inicio = state->inicioFalha;
      n.rssi = rssi;
      queueNotification(n);
      state->downNotificationSent = true;
   }
}

void onLinkUp(LinkState *state, LinkId link, int16_t rssi) {

   uint32_t duracao;
   time_t fim = now();

   state->ultimaMudanca = fim;
   state->ultimoRSSI = rssi;

   DBG("\nEvento #%u encerrado\n", state->eventoAtual);
   DBG("Operadora : %s\n", gPerfil->links[link].nome);
   DBG("Motivo    : %s\n", linkStatusDescription(state->status));
   if (state->inicioFalha != 0) {
      duracao = (uint32_t)(fim - state->inicioFalha);
      DBG("Inicio    : %s\n", formatDateTime(state->inicioFalha).c_str());
   } else {
      duracao = 0;
      DBG("Inicio da falha desconhecido.\n");
   }
   DBG("Fim       : %s\n", formatDateTime(fim).c_str());
   DBG("Duracao   : %s\n", formatDuration(duracao).c_str());

   if (state->downNotificationSent && !inQuietHours()) {
      PendingNotification n;
      n.pending = true;
      n.link = link;
      n.motivo = state->status;
      n.tipo = NOTIFY_UP;
      n.evento = state->eventoAtual;
      n.inicio = state->inicioFalha;
      n.fim = fim;
      n.duracao = duracao;
      n.rssi = rssi;

      queueNotification(n);
   }

   Event ev = {};
   ev.id = state->eventoAtual;
   ev.link = link;
   ev.motivo = state->status;
   ev.inicio = state->inicioFalha;
   ev.fim = fim;
   ev.duracaoSeg = duracao;
   ev.rssi = rssi;
   appendEvent(ev);

   state->eventoAtual = 0;
   state->inicioFalha = 0;
}