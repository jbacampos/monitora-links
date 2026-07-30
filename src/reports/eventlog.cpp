#include "reports/eventlog.h"

#include "config/config.h"
#include "core/storage.h"

//=============================================================================
// Consulta ao histórico
//=============================================================================

uint16_t getEventCount() { return gEvents.eventCounter; }

const Event *getEvent(uint16_t index) {
   if (index >= gEvents.eventCounter)
      return nullptr;

   uint16_t pos = (gEvents.firstEventId + index) % MAX_EVENTS;

   return &gEvents.events[pos];
}

//=============================================================================
// Gravação
//=============================================================================

void appendEvent(const Event &evento) {
   uint16_t pos = (gEvents.firstEventId + gEvents.eventCounter) % MAX_EVENTS;

   gEvents.events[pos] = evento;
   
   if (gEvents.eventCounter < MAX_EVENTS) {
      gEvents.eventCounter++;
   } else {
      gEvents.firstEventId = (gEvents.firstEventId + 1) % MAX_EVENTS;
   }

   if (saveStorage(FILE_EVENTS, gEvents)) {
      DBG("Evento gravado. Total: %u\n", gEvents.eventCounter);
   } else {
      DBG("ERRO gravando historico de eventos.\n");
   }
}