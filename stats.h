#ifndef STATS_H
#define STATS_H

#include "types.h"

//=============================================================================
// Estatísticas
//=============================================================================

uint16_t getFailureCount(LinkId link, uint16_t dias);

uint32_t getDowntime(LinkId link, uint16_t dias);

uint32_t getLongestFailure(LinkId link, uint16_t dias);

uint32_t getAverageFailure(LinkId link, uint16_t dias);

float getUptime(LinkId link, uint16_t dias);

#endif