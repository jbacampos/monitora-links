#include "types.h"

//=============================================================================
// Conversão de status
//=============================================================================

const char *linkStatusName(LinkStatus status) {
   switch (status) {

   case LINK_ONLINE:
      return "ONLINE";

   case LINK_WIFI_FAIL:
      return "WIFI_FAIL";

   case LINK_INTERNET_FAIL:
      return "INTERNET_FAIL";

   default:
      return "UNKNOWN";
   }
}

const char *linkStatusDescription(LinkStatus status) {
   switch (status) {

   case LINK_ONLINE:
      return "Online";

   case LINK_WIFI_FAIL:
      return "Wi-Fi indisponivel";

   case LINK_INTERNET_FAIL:
      return "Sem internet";

   default:
      return "Desconhecido";
   }
}