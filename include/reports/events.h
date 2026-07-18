#ifndef EVENTS_H
#define EVENTS_H

#include "core/types.h"

//=============================================================================
// Transição de estado dos links
//=============================================================================

void onLinkDown(LinkState *state, LinkId link, LinkStatus motivo, int16_t rssi);

void onLinkUp(LinkState *state, LinkId link, int16_t rssi);

#endif