#ifndef EVENTLOG_H
#define EVENTLOG_H

#include "core/types.h"

//=============================================================================
// Consulta ao histórico
//=============================================================================

uint16_t getEventCount();

bool getEvent(uint16_t index, Event& evento);
void initEventsMutex();

//=============================================================================
// Gravação
//=============================================================================

void appendEvent(const Event &evento);
uint32_t reserveEventId(bool persist);

#endif