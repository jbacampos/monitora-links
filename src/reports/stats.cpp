#include "reports/stats.h"

#include "config/config.h"
#include "reports/eventlog.h"
#include "network/ntp.h"

//=============================================================================
// Estatísticas
//=============================================================================

static time_t calculaPeriodo(uint16_t dias, time_t *agora) {
   if (!clockIsValid()) {
      return 0;
   }

   *agora = now();
   return *agora - (time_t)dias * SECONDS_PER_DAY;
}

static bool eventoValido(const Event *ev) {
   return ev->inicio != 0;
}

uint32_t getDowntime(LinkId link, uint16_t dias) {
   time_t agora;
   time_t inicio = calculaPeriodo(dias, &agora);
   if (inicio == 0) {
      return 0;
   }

   uint32_t total = 0;
   time_t ultimoFim = inicio;

   uint16_t first = gEvents.firstEventId % MAX_EVENTS;

   for (uint16_t k = 0; k < gEvents.eventCounter; k++) {
      const Event *ev = &gEvents.events[(first + k) % MAX_EVENTS];

      if (ev->link != link) {
         continue;
      }

      if (!eventoValido(ev)) {
         continue;
      }

      time_t a = ev->inicio;
      time_t b = ev->fim;

      // Recorta o evento à janela [inicio, agora].
      if (a < inicio) {
         a = inicio;
      }

      if (b > agora) {
         b = agora;
      }

      if (b > a) {
         if (a > ultimoFim) {
            total += (uint32_t)(b - a);
         } else if (b > ultimoFim) {
            total += (uint32_t)(b - ultimoFim);
         }

         if (b > ultimoFim) {
            ultimoFim = b;
         }
      }
   }

   // Falha atualmente ativa: ainda não existe Event fechado no histórico.
   {
      const LinkState *ls = &gRuntime.links[link];

      if (ls->eventoAtual != 0 && ls->inicioFalha != 0) {
         time_t a = ls->inicioFalha;
         time_t b = agora;

         if (a < inicio) {
            a = inicio;
         }

         if (b > a) {
            if (a > ultimoFim) {
               total += (uint32_t)(b - a);
            } else if (b > ultimoFim) {
               total += (uint32_t)(b - ultimoFim);
            }

            if (b > ultimoFim) {
               ultimoFim = b;
            }
         }
      }
   }

   return total;
}

uint16_t getFailureCount(LinkId link, uint16_t dias) {

   if (!clockIsValid())
      return 0;

   time_t limite = now() - (time_t)dias * SECONDS_PER_DAY;
   uint16_t total = 0;

   uint16_t totalEventos = getEventCount();

   for (uint16_t i = 0; i < totalEventos; i++) {
      Event ev;
      if (!getEvent(i, ev))
         continue;
      if (ev.link != link)
         continue;
      if (ev.fim < limite)
         continue;
      total++;
   }

   return total;
}

uint32_t getLongestFailure(LinkId link, uint16_t dias) {

   if (!clockIsValid())
      return 0;

   time_t limite = now() - (time_t)dias * SECONDS_PER_DAY;
   uint32_t maior = 0;

   uint16_t totalEventos = getEventCount();

   for (uint16_t i = 0; i < totalEventos; i++) {
      Event ev;
      if (!getEvent(i, ev))
         continue;
      if (ev.link != link)
         continue;
      if (ev.inicio == 0)
         continue;
      if (ev.inicio < limite)
         continue;
      if (ev.duracaoSeg > maior)
         maior = ev.duracaoSeg;
   }

   return maior;
}

uint32_t getAverageFailure(LinkId link, uint16_t dias) {

   if (!clockIsValid())
      return 0;

   time_t limite = now() - (time_t)dias * SECONDS_PER_DAY;
   uint32_t totalFalhas = 0, totalTempo = 0;

   uint16_t totalEventos = getEventCount();

   for (uint16_t i = 0; i < totalEventos; i++) {
      Event ev;
      if (!getEvent(i, ev))
         continue;
      if (ev.link != link)
         continue;
      if (ev.inicio == 0)
         continue;
      if (ev.inicio < limite)
         continue;
      totalFalhas++;
      totalTempo += ev.duracaoSeg;
   }
   return (totalFalhas == 0) ? 0 : totalTempo / totalFalhas;
}

float getUptime(LinkId link, uint16_t dias) {
   uint32_t tempoPeriodo = dias * SECONDS_PER_DAY;
   uint32_t tempoFora = getDowntime(link, dias);

   return 100.0f * (tempoPeriodo - tempoFora) / tempoPeriodo;
}
