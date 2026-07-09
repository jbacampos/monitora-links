#ifndef MONITOR_H
#define MONITOR_H

#include "types.h"

void processLink(LinkState *state, LinkId link, LinkStatus current,
                 int16_t rssi);

#endif
