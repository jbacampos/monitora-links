#include "core/types.h"

//=============================================================================
// Conversão de status
//=============================================================================

const char *linkStatusDescription(LinkStatus status) {
   switch (status) {

   case LINK_ONLINE:
      return "Online";

   case LINK_WIFI_FAIL:
      return "Sem wi-Fi";

   case LINK_INTERNET_FAIL:
      return "Sem internet";

   default:
      return "Desconhecido";
   }
}

const char *bootReasonDescription(BootReason reason) {
   switch (reason) {

   case BOOT_POWERON:
      return "Energizado";

   case BOOT_AFTER_REBOOT_COMMAND:
      return "Comando /reboot";

   case BOOT_AFTER_OTA:
      return "Atualização OTA";

   default:
      return "Desconhecido";
   }
}
