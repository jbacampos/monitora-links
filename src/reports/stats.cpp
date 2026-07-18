#include "reports/stats.h"

#include "config/config.h"
#include "reports/eventlog.h"
#include "network/ntp.h"

//=============================================================================
// Estatísticas
//=============================================================================

uint32_t getDowntime(LinkId link, uint16_t dias) {

   if (!clockIsValid())
      return 0;

   time_t limite = now() - (time_t)dias * SECONDS_PER_DAY;
   uint32_t total = 0;
   uint16_t totalEventos = getEventCount();

   for (uint16_t i = 0; i < totalEventos; i++) {
      const Evento *ev = getEvent(i);
      if (ev->link != link)
         continue;
      if (ev->fim < limite)
         continue;
      total += ev->duracaoSeg;
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
      const Evento *ev = getEvent(i);
      if (ev->link != link)
         continue;
      if (ev->fim < limite)
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
      const Evento *ev = getEvent(i);
      if (ev->link != link)
         continue;
      if (ev->inicio == 0)
         continue;
      if (ev->inicio < limite)
         continue;
      if (ev->duracaoSeg > maior)
         maior = ev->duracaoSeg;
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
      const Evento *ev = getEvent(i);
      if (ev->link != link)
         continue;
      if (ev->inicio == 0)
         continue;
      if (ev->inicio < limite)
         continue;
      totalFalhas++;
      totalTempo += ev->duracaoSeg;
   }
   return (totalFalhas == 0) ? 0 : totalTempo / totalFalhas;
}

float getUptime(LinkId link, uint16_t dias) {
   uint32_t tempoPeriodo = dias * SECONDS_PER_DAY;
   uint32_t tempoFora = getDowntime(link, dias);

   return 100.0f * (tempoPeriodo - tempoFora) / tempoPeriodo;
}
