#include "reports/eventlog.h"

#include "config/config.h"
#include "core/storage.h"

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t eventsMutex = nullptr;

void initEventsMutex() {

   eventsMutex = xSemaphoreCreateMutex();

   if (eventsMutex != nullptr)
      DBG("Mutex dos eventos criado\n");
   else
      DBG("ERRO ao criar mutex dos eventos\n");
}

static void lockEvents() {

   if (eventsMutex != nullptr)
      xSemaphoreTake(eventsMutex, portMAX_DELAY);
}

static void unlockEvents() {

   if (eventsMutex != nullptr)
      xSemaphoreGive(eventsMutex);
}

//=============================================================================
// Consulta ao histórico
//=============================================================================

uint16_t getEventCount() {

   lockEvents();

   uint16_t count = gEvents.eventCounter;

   unlockEvents();

   return count;
}

bool getEvent(uint16_t index, Event& evento) {

   bool result = false;

   lockEvents();

   if (index < gEvents.eventCounter) {
      result = true;
      uint16_t pos = (gEvents.firstEventId + index) % MAX_EVENTS;  
      evento = gEvents.events[pos];
   }  

   unlockEvents();

   return result;
}


//=============================================================================
// Gravação
//=============================================================================

void appendEvent(const Event &evento) {

   lockEvents();

   uint16_t pos = (gEvents.firstEventId + gEvents.eventCounter) % MAX_EVENTS;

   gEvents.events[pos] = evento;

   if (gEvents.eventCounter < MAX_EVENTS) {
      gEvents.eventCounter++;
   } else {
      gEvents.firstEventId = (gEvents.firstEventId + 1) % MAX_EVENTS;
   }

   gEvents.saveCount++;

   bool saved = saveStorage(FILE_EVENTS, gEvents);

   if (saved) {
      DBG("Evento gravado. Total: %u\n",
          gEvents.eventCounter);
   } else {
      gEvents.saveCount--;
      DBG("ERRO gravando historico de eventos.\n");
   }

   unlockEvents();
}


uint32_t reserveEventId(bool persist) {

   lockEvents();

   gEvents.lastEventId++;
   if (persist) {
      gEvents.saveCount++;
      if (!saveStorage(FILE_EVENTS, gEvents)) {
         gEvents.saveCount--;
         DBG("ERRO persistindo ID do evento.\n");
      }
   }

   uint32_t id = gEvents.lastEventId;

   unlockEvents();

   return id;
}