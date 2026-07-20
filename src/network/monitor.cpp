#include "network/monitor.h"
#include "config/config.h"
#include "reports/events.h"
#include "network/wifi_manager.h"
#include "network/ntp.h"
#include "core/types.h"

void processLink(LinkState *state, LinkId link, LinkStatus current, int16_t rssi) {

   DBG("Status : %s -> %s", linkStatusName(state->status),
       linkStatusName(current));

   if (current == state->status) {
      DBG(" (igual)\n");
      state->ultimoRSSI = rssi;
      return;
   }

   DBG(" (MUDOU)\n");

   if (current == LINK_ONLINE) {
      onLinkUp(state, link, rssi);
   } else {
      onLinkDown(state, link, current, rssi);
   }

   state->status = current;
   state->ultimoRSSI = rssi;
}