#include "network/ntp.h"

#include <Arduino.h>

#include "config/config.h"

//=============================================================================
// Sincronização
//=============================================================================

bool syncClock() {
   configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

   setenv("TZ", "BRT3", 1);
   tzset();

   DBG("Relógio: ");

   for (int i = 0; i < 10; i++) {
      time_t agora = time(nullptr);

      if (agora > 1700000000) {
         DBG(formatDateTime(agora, DATETIME_FULL).c_str());
         DBG("\n");
         return true;
      }
      delay(1000);
   }

   DBG(" desconhecido\n");
   return false;
}

//=============================================================================
// Estado do relógio
//=============================================================================

bool clockIsValid() { return (time(nullptr) > 1700000000); }

time_t now() { return time(nullptr); }

//=============================================================================
// Formatação
//=============================================================================

String formatDateTime(time_t t, DateTimeFormat format) {

   static const char *const meses[] = {"jan", "fev", "mar", "abr",
                                       "mai", "jun", "jul", "ago",
                                       "set", "out", "nov", "dez"};

   if (t == 0)
      return "desconhecido";

   t += GMT_OFFSET_SEC;
   struct tm tm;
   gmtime_r(&t, &tm);
   char texto[30];

   if (format == DATETIME_FULL) {
      snprintf(texto, sizeof(texto), "%02d/%02d/%04d %02d:%02d:%02d",
               tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour,
               tm.tm_min, tm.tm_sec);
   } else {
      snprintf(texto, sizeof(texto), "%02d/%s - %02d:%02d", tm.tm_mday,
               meses[tm.tm_mon], tm.tm_hour, tm.tm_min);
   }

   return String(texto);
}

String formatDuration(uint32_t seconds) {
   uint32_t h = seconds / 3600;
   uint32_t m = (seconds % 3600) / 60;
   uint32_t s = seconds % 60;


   char texto[16];

   snprintf(texto, sizeof(texto), "%02u:%02u:%02u", h, m, s);

   return String(texto);
}