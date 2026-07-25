#ifndef EVENTLOG_H
#define EVENTLOG_H

#include "core/types.h"

//=============================================================================
// Consulta ao histórico
//=============================================================================

uint16_t getEventCount();

const Event *getEvent(uint16_t index);

//=============================================================================
// Gravação
//=============================================================================

void appendEvent(const Event &evento);

#endif