#ifndef NTP_H
#define NTP_H

#include <time.h>

#include "types.h"

//=============================================================================
// Sincronização
//=============================================================================

bool syncClock();

//=============================================================================
// Estado do relógio
//=============================================================================

bool clockIsValid();

time_t now();

//=============================================================================
// Formatação
//=============================================================================

enum DateTimeFormat { DATETIME_FULL, DATETIME_SHORT };

String formatDateTime(time_t t, DateTimeFormat format = DATETIME_FULL);
String formatDuration(uint32_t seconds);

#endif