#ifndef MONITOR_H
#define MONITOR_H

#include "core/types.h"

void processLinkState(LinkState *state, LinkId link, LinkStatus current,
                 int16_t rssi);

#endif
