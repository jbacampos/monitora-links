#include "reports/reports.h"

#include "config/config.h"
#include "reports/eventlog.h"
#include "network/notify.h"
#include "network/ntp.h"
#include "reports/stats.h"

//=============================================================================
// Status
//=============================================================================

String buildStatus() {
   String msg = "\n==========================\n";
   msg += "Status do sistema\n";
   msg += "==========================\n\n";

   msg += "<b><u>Links\n\n</u></b>";

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      const LinkConfig &cfg = gPerfil->links[i];
      const LinkState &link = gRuntime.links[i];

      if (link.status == LINK_ONLINE) {
         msg += EmojiOnline;
         msg += " ";
         msg += cfg.nome;
         msg += "  <b>ONLINE</b>\n";
         msg += "RSSI   : ";
         msg += String(link.ultimoRSSI);
         msg += " dBm\n";
      } else {
         msg += EmojiOffline;
         msg += " ";
         msg += cfg.nome;
         msg += "  <b>OFFLINE</b>\n";
         msg += "Motivo : ";
         msg += linkStatusDescription(link.status);
         msg += "\n";
         msg += "Inicio : ";

         if (link.inicioFalha != 0)
            msg += formatDateTime(link.inicioFalha, DATETIME_SHORT);
         else
            msg += "desconhecido";

         msg += "\n";
      }

      if (i < gPerfil->numLinks - 1)
         msg += "\n";
   }

   return msg;
}

//=============================================================================
// Histórico
//=============================================================================

// String buildLog_antigo (uint16_t maxEventos) {
//    String msg = "\n==========================\n";
//    msg += "Eventos - últimos ";
//    msg += String(maxEventos);
//    msg += "\n==========================\n";

//    uint16_t total = getEventCount();

//    if (total == 0)
//       return msg + "Nenhum evento.\n";

//    uint16_t inicio = (total > maxEventos) ? total - maxEventos : 0;

//    for (uint16_t i = inicio; i < total; i++) {

//       const Event* ev = getEvent(i);
//       if (ev == nullptr) {
//          msg += "\nERRO: getEvent retornou nullptr\n";
//          continue;
//       }
//       const LinkConfig& cfg = gPerfil->links[ev->link];

//       msg += "\n#";
//       msg += String(ev->id);
//       msg += " ";
//       if (ev->tipo == EVENT_BOOT) {
//          msg += "Monitor reiniciado";
//       } else {
//          msg += cfg.nome;
//       }

//       msg += "\n";

//       msg += "Motivo : ";
//       if (ev->tipo == EVENT_BOOT) {
//          msg += bootReasonDescription(ev->bootReason);
//       } else {
//          msg += linkStatusDescription(ev->motivo);
//       }
//       msg += "\n";

//       msg += "Inicio : ";

//       if (ev->inicio != 0)
//          msg += formatDateTime(ev->inicio, DATETIME_SHORT);
//       else
//          msg += "desconhecido";

//       msg += "\n";

//       msg += "Duracao: ";
//       msg += formatDuration(ev->duracaoSeg);
//       msg += "\n";
//    }

//    msg += "\n";

//    return msg;
// }

String buildLog(uint16_t maxEventos) {
   String msg = "\n==========================\n";
   msg += "Eventos - últimos ";
   msg += String(maxEventos);
   msg += "\n==========================\n";

   uint16_t total = getEventCount();

   if (total == 0)
      return msg + "Nenhum evento.\n";

   uint16_t quantidade = min(total, maxEventos);

   // Índices dos eventos que serão exibidos
   uint16_t indices[MAX_EVENTS];

   for (uint16_t i = 0; i < total; i++)
      indices[i] = i;

   // Ordena os índices pelo ID do evento
   for (uint16_t i = 0; i < total - 1; i++) {
      for (uint16_t j = i + 1; j < total; j++) {

         const Event* a = getEvent(indices[i]);
         const Event* b = getEvent(indices[j]);

         if (a != nullptr && b != nullptr && a->id > b->id) {
            uint16_t temp = indices[i];
            indices[i] = indices[j];
            indices[j] = temp;
         }
      }
   }

   // Depois da ordenação, os maiores IDs estão no final
   uint16_t inicio = total - quantidade;

   for (uint16_t i = inicio; i < total; i++) {

      const Event* ev = getEvent(indices[i]);

      if (ev == nullptr) {
         msg += "\nERRO: getEvent retornou nullptr\n";
         continue;
      }

      msg += "\n#";
      msg += String(ev->id);
      msg += " ";

      if (ev->tipo == EVENT_BOOT) {
         msg += "Monitor reiniciado";
      } else {
         const LinkConfig& cfg = gPerfil->links[ev->link];
         msg += cfg.nome;
      }

      msg += "\n";

      msg += "Motivo : ";

      if (ev->tipo == EVENT_BOOT) {
         msg += bootReasonDescription(ev->bootReason);
      } else {
         msg += linkStatusDescription(ev->motivo);
      }

      msg += "\n";

      if (ev->inicio != 0) {
         msg += "Inicio      :  ";
         msg += formatDateTime(ev->inicio, DATETIME_SHORT);
         msg += "\n";

         msg += "Duracao :  ";
         msg += formatDuration(ev->duracaoSeg);
         msg += "\n";

      } else {
         msg += "Em         :  ";
         msg += formatDateTime(ev->fim, DATETIME_SHORT);
         msg += "\n";

      }

   }

   msg += "\n";

   return msg;
}
//=============================================================================
// Notificações e led
//=============================================================================

String buildInfoNotif() {
   String msg = "\nEnvio de notificações:\n";
   msg += (gConfig.notification.enabled) ? "ATIVO" : "INATIVO";
   msg += "\nPeríodo quieto (PQ):\n";
   msg += (gConfig.notification.quietEnabled) ? "ATIVO - " : "INATIVO - ";
   msg += formatTime(gConfig.notification.quietStart);
   msg += " - ";
   msg += formatTime(gConfig.notification.quietEnd);
   return msg;
}

String buildInfoLed() {
   String msg = "\nLed:\n";
   if (gConfig.notification.ledMode == LED_MODE_OFF)
      msg += "⚪ INATIVO sempre";
   else if (gConfig.notification.ledMode == LED_MODE_ON)
      msg += "🔵 ATIVO sempre";
   else
      msg += "⚪🔵 INATIVO no PQ; ATIVO fora";
   return msg;
}

//=============================================================================
// Estatísticas
//=============================================================================

String buildStatistics(uint16_t dias) {
   char uptime[8];

   String msg = "==========================\n";
   msg += "Estatísticas - ";

   if (dias == 1)
      msg += "últimas 24 horas\n";
   else
      msg += "últimos " + String(dias) + " dias\n";

   msg += "==========================\n";

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {
      const LinkConfig &cfg = gPerfil->links[i];

      msg += "\n=== ";
      msg += cfg.nome;
      msg += " ===\n";

      msg += "Falhas : ";
      msg += String(getFailureCount(i, dias));

      msg += "\nTempo  : ";
      msg += formatDuration(getDowntime(i, dias));

      msg += "\nMaior  : ";
      msg += formatDuration(getLongestFailure(i, dias));

      msg += "\nMedia  : ";
      msg += formatDuration(getAverageFailure(i, dias));

      msg += "\nUptime : ";
      snprintf(uptime, sizeof(uptime), "%.2f%%", getUptime(i, dias));
      msg += uptime;
      msg += "\n";
   }

   msg += "\n";

   return msg;
}

//=============================================================================
// Resumo do sistema
//=============================================================================

String buildSystemSummary() {
   String msg;

   msg += "\n<b><u>Notificações</u></b>";
   msg += buildInfoNotif();
   msg += buildInfoLed();

   msg += "\n\n<b><u>Sistema</u></b>";
   msg += "\n  Firmware:    ";
   msg += FW_VERSION;

   msg += "\n  Placa:             ";
   msg += getHardwareName();

   msg += "\n  Boots:           ";
   msg += formatNumber(gRuntime.bootCount).c_str();

   msg += "\n  Ciclos:           ";
   msg += formatNumber(gCycleCount).c_str();

   msg += "\n  Horário:        ";
   time_t agora = time(nullptr);
   if (agora > 1700000000) {
      msg += formatDateTime(agora, DATETIME_SHORT).c_str();
   } else {
      msg += "ainda não ajustado";
   }

   uint32_t heap = ESP.getFreeHeap();
   msg += "\n  Free heap:    ";
   msg += prettySize(heap);
   String obs;
   if (heap > 35 * 1024)
      obs = " (excelente)";
   else if (heap > 30 * 1024)
      obs = " (muito bom)";
   else if (heap > 25 * 1024)
      obs = " (bom)";
   else if (heap > 20 * 1024)
      obs = " (atenção)";
   else if (heap > 15 * 1024)
      obs = " (baixo)";
   else
      obs = " (crítico)";
   msg += obs;

#ifdef ESP8266
   FSInfo info;
   LittleFS.info(info);
   uint32_t total = info.totalBytes;
   uint32_t used  = info.usedBytes;
#elif defined(ESP32)
   uint32_t total = LittleFS.totalBytes();
   uint32_t used  = LittleFS.usedBytes();
#endif


   msg += "\n\n<b><u>Armazenamento</u></b>";
   msg += "\n<b>LittleFS</b>";
   msg += "\n  Usado:                    ";
   msg += prettySize(used);
   msg += "\n  Total:                       ";
   msg += prettySize(total);
   msg += "\n  Livre:                       ";
   msg += prettySize(total - used);

   msg += "\n<b>Memória Flash</b>";
   msg += "\n  Configurada:        ";
   msg += prettySize(ESP.getFlashChipSize());
   msg += "\n  Sketch:                   ";
   msg += prettySize(ESP.getSketchSize());
   msg += ("\n  Livre p/ OTA:        ");
   msg += prettySize(ESP.getFreeSketchSpace());
   msg += ("\n  Gravações:            ");
   msg += formatNumber(gRuntime.saveCount + gEvents.saveCount + gConfig.saveCount).c_str();
   msg += ("\n");

   return msg;
}

String getHardwareName() {

#ifdef ESP8266
   return "ESP8266";
#elif defined(ESP32)
   return "ESP32";
#else
   return "Desconhecido";
#endif
}

String prettySize(uint32_t bytes) {
   if (bytes < 1024)
      return String(bytes) + " B";

   double value = bytes;
   const char *unit = "KB";

   value /= 1024.0;

   if (value >= 1024.0) {
      value /= 1024.0;
      unit = "MB";
   }

   if (value >= 1024.0) {
      value /= 1024.0;
      unit = "GB";
   }

   return String(value, 1) + " " + unit;
}

String formatNumber(uint32_t value) {
   String s = String(value);

   for (int i = s.length() - 3; i > 0; i -= 3)
      s = s.substring(0, i) + '.' + s.substring(i);

   return s;
}

String formatTime(uint16_t min) {

   uint8_t h = min / 60;
   uint8_t m = min % 60;

   char texto[10];
   snprintf(texto, sizeof(texto), "%02u:%02u", h, m);

   return String(texto);
}

