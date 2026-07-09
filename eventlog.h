#ifndef EVENTLOG_H
#define EVENTLOG_H

#include "types.h"

//=============================================================================
// Consulta ao histórico
//=============================================================================

uint16_t getEventCount();

const Evento *getEvent(uint16_t index);

//=============================================================================
// Gravação
//=============================================================================

void appendEvent(const Evento &evento);

#endif