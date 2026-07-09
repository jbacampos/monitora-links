#include "eventlog.h"

#include "config.h"
#include "storage.h"

//=============================================================================
// Consulta ao histórico
//=============================================================================

uint16_t getEventCount() { return gState.numeroEventos; }

const Evento *getEvent(uint16_t index) {
   if (index >= gState.numeroEventos)
      return nullptr;

   uint16_t pos = (gState.primeiroEvento + index) % MAX_EVENTS;

   return &gState.eventos[pos];
}

//=============================================================================
// Gravação
//=============================================================================

void appendEvent(const Evento &evento) {
   uint16_t pos = (gState.primeiroEvento + gState.numeroEventos) % MAX_EVENTS;

   gState.eventos[pos] = evento;

   if (gState.numeroEventos < MAX_EVENTS) {
      gState.numeroEventos++;
   } else {
      gState.primeiroEvento = (gState.primeiroEvento + 1) % MAX_EVENTS;
   }

   if (saveState(&gState)) {
      DBG("Evento gravado. Total: %u\n", gState.numeroEventos);
   } else {
      DBG("ERRO gravando historico de eventos.\n");
   }
}