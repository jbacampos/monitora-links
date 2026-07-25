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
// Persistência
//=============================================================================

bool loadState(PersistState* st) {
   File f = LittleFS.open(FILE_STATE, "r");

   if (!f) {
      DBG("state.bin inexistente.\n");
      return false;
   }

   if (f.size() != sizeof(PersistState)) {
      DBG("Tamanho de state.bin incompatível\n");
      DBG("Arquivo : %u bytes\n", (unsigned)f.size());
      DBG("Esperado: %u bytes\n", sizeof(PersistState));

      f.close();
      return false;
   }

   size_t lidos = f.read((uint8_t*)st, sizeof(PersistState));

   f.close();

   if (lidos != sizeof(PersistState)) {
      DBG("Bytes lidos não batem com o tamanho de PersistState\n");
      DBG("Lidos.......: %u\n", lidos);
      DBG("Esperado....: %u\n", sizeof(PersistState));
      return false;
   }

   if (st->magic != MAGIC_NUMBER) {
      DBG("MAGIC_NUMBER inválido.\n");
      DBG("Lido........: %u\n", st->magic);
      DBG("Esperado....: %u\n", MAGIC_NUMBER);
      return false;
   }

   if (st->version != STATE_VERSION) {
      DBG("STATE_VERSION incompatível.\n");
      DBG("Lido........: %u\n", st->version);
      DBG("Esperado....: %u\n", STATE_VERSION);
      return false;
   }

   return true;
}


//=============================================================================
// Administração
//=============================================================================

void resetState() {
   LittleFS.remove(FILE_STATE);
   createDefaultState();
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

   for (uint8_t i = 0; i < NUM_LINKS; i++) {
      gRuntime.links[i].status = LINK_UNKNOWN;
   }

   saveStorage(FILE_RUNTIME, gRuntime);
}

void createDefaultEvents() {
   memset(&gEvents, 0, sizeof(gEvents));

   gEvents.header.magic = EVENTS_MAGIC;
   gEvents.header.version = EVENTS_VERSION;

   gEvents.lastEventId = 1;

   saveStorage(FILE_EVENTS, gEvents);
}
