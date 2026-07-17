#include "storage.h"

#include "config.h"

//=============================================================================
// Estado global
//=============================================================================

PersistState gState;

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

bool loadState(PersistState *st) {
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

   size_t lidos = f.read((uint8_t *)st, sizeof(PersistState));
   
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

bool saveState(PersistState *st) {
   File f = LittleFS.open(FILE_STATE, "w");

   if (!f) {
      DBG("Não foi possível gravar /state.bin\n");
      return false;
   }

   size_t gravados = f.write((uint8_t *)st, sizeof(PersistState));

   f.close();

   if (gravados != sizeof(PersistState)) {
      DBG("Bytes gravados não batem com o tamanho de PersistState\n");
      DBG("Gravados : %u\n", gravados);
      DBG("PersistState: %u\n", sizeof(PersistState));
      return false;
   }

   gState.saveCount++;
   return true;
}

//=============================================================================
// Administração
//=============================================================================

// Inicializa um novo estado persistente e grava state.bin.
void createDefaultState() {
   memset(&gState, 0, sizeof(gState));

   gState.magic = MAGIC_NUMBER;
   gState.version = STATE_VERSION;

   gState.proximoEvento = 1;

   for (uint8_t i = 0; i < NUM_LINKS; i++) {
      gState.links[i].status = LINK_ONLINE;
   }

   gState.notification.enabled = NOTIFICATIONS_ENABLED_DEFAULT;
   gState.notification.quietEnabled = QUIET_HOURS_ENABLED_DEFAULT;
   gState.notification.quietStart = QUIET_HOURS_START_DEFAULT;
   gState.notification.quietEnd = QUIET_HOURS_END_DEFAULT;
   // gState.notification.summaryEnabled = SUMMARY_ENABLED_DEFAULT;
   // gState.notification.summaryTime    = SUMMARY_TIME_DEFAULT;
   // gState.notification.summaryDay     = 0xFFFF;
   gState.notification.ledMode = LED_MODE_DEFAULT;

   DBG("Criando novo state.bin\n");
   DBG("gState.version: %u\n", gState.version);

   saveState(&gState);
}

void resetState() {
   LittleFS.remove(FILE_STATE);
   createDefaultState();
}