
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

void onLinkDown(LinkState *state, LinkId link, LinkStatus motivo, int16_t rssi) {

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
      PendingNotification n = {};
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
   ev.tipo = EVENT_LINK;
   appendEvent(ev);

   state->eventoAtual = 0;
   state->inicioFalha = 0;
}

void onBoot() {

   uint32_t duracao;
   time_t inicio = 0;
   time_t fim = now();

   DBG("\nMonitor reiniciado. Motivo: %s\n", bootReasonDescription(gRuntime.bootReason));
   if ((gRuntime.bootReason == BOOT_AFTER_REBOOT_COMMAND || gRuntime.bootReason == BOOT_AFTER_OTA) && gRuntime.rebootStartTime != 0) {
      inicio = gRuntime.rebootStartTime;
      duracao = (uint32_t)(fim - inicio);
      DBG("Inicio    : %s\n", formatDateTime(gRuntime.rebootStartTime).c_str());
   } else {
      inicio = 0;
      duracao = 0;
      DBG("Inicio desconhecido.\n");
   }
   DBG("Fim       : %s\n", formatDateTime(fim).c_str());
   DBG("Duracao   : %s\n", formatDuration(duracao).c_str());
   
   Event ev = {};
   gEvents.lastEventId++;
   ev.id = gEvents.lastEventId;
   ev.link = LINK_SYSTEM;
   ev.bootReason = gRuntime.bootReason;
   ev.inicio = inicio;
   ev.fim = fim;
   ev.duracaoSeg = duracao;
   ev.rssi = 0;
   ev.tipo = EVENT_BOOT;
   appendEvent(ev);

   PendingNotification n = {};
   n.pending = true;
   n.link = LINK_SYSTEM;
   n.bootReason = gRuntime.bootReason;
   n.tipo = NOTIFY_BOOT;
   n.evento = gEvents.lastEventId;
   n.inicio = inicio;
   n.fim = fim;
   n.duracao = duracao;
   n.rssi = 0;

   queueNotification(n);

   gRuntime.bootReason = BOOT_POWERON;
   gRuntime.rebootStartTime = now();
   saveStorage(FILE_RUNTIME, gRuntime);

   
}