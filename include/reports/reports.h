#ifndef REPORTS_H
#define REPORTS_H

#include <LittleFS.h>

#include "core/types.h"

//=============================================================================
// Relatórios
//=============================================================================

String buildStatus();

String buildLog(uint16_t maxEventos);

String buildStatistics(uint16_t dias);
String buildSystemSummary();
String buildInfoNotif();
String buildInfoLed();
String getHardwareName();
String prettySize(uint32_t bytes);
String formatNumber(uint32_t value);
String formatTime(uint16_t min);


#endif