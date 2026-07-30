#include "core/storage.h"

#include "config/config.h"

//=============================================================================
// Estado global
//=============================================================================

ConfigData gConfig;
RuntimeData gRuntime;
EventsData gEvents;

//=============================================================================
// Sistema de arquivos
//=============================================================================

bool initFS() {

#ifdef ESP8266
   return LittleFS.begin();
#elif defined(ESP32)
   return LittleFS.begin(true);
#endif
}


//=============================================================================
// Administração
//=============================================================================

void resetConfig() {
   LittleFS.remove(FILE_CONFIG);
   createDefaultConfig();
}

void resetRuntime() {
   LittleFS.remove(FILE_RUNTIME);
   createDefaultRuntime();
}

void resetEvents() {
   LittleFS.remove(FILE_EVENTS);
   createDefaultEvents();
}

void createDefaultConfig()
{
   memset(&gConfig, 0, sizeof(gConfig));

   gConfig.header.magic = CONFIG_MAGIC;
   gConfig.header.version = CONFIG_VERSION;

   gConfig.notification.enabled = NOTIFICATIONS_ENABLED_DEFAULT;
   gConfig.notification.quietEnabled = QUIET_HOURS_ENABLED_DEFAULT;
   gConfig.notification.quietStart = QUIET_HOURS_START_DEFAULT;
   gConfig.notification.quietEnd = QUIET_HOURS_END_DEFAULT;
   gConfig.notification.ledMode = LED_MODE_DEFAULT;

   saveStorage(FILE_CONFIG, gConfig);
}

void createDefaultRuntime() {
   memset(&gRuntime, 0, sizeof(gRuntime));

   gRuntime.header.magic = RUNTIME_MAGIC;
   gRuntime.header.version = RUNTIME_VERSION;

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      gRuntime.links[i].status = LINK_ONLINE;
   }

   saveStorage(FILE_RUNTIME, gRuntime);
}

void createDefaultEvents() {
   memset(&gEvents, 0, sizeof(gEvents));

   gEvents.header.magic = EVENTS_MAGIC;
   gEvents.header.version = EVENTS_VERSION;

   saveStorage(FILE_EVENTS, gEvents);
}
