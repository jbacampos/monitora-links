#include "network/telegram.h"

#include <Arduino.h>
#include <WiFiClientSecure.h>

#include "config/config.h"
#include "config/platform.h"
#include "core/storage.h"
#include "network/notify.h"
#include "network/ntp.h"
#include "network/ota.h"
#include "network/wifi_manager.h"
#include "reports/reports.h"

//=============================================================================
// Tipos internos
//=============================================================================

typedef CommandResult (*CommandHandler)(const String &args);

typedef struct {
   const char *comando;
   CommandHandler handler;
} TelegramCommand;

//=============================================================================
// Protótipos privados
//=============================================================================

static bool telegramParseUpdate(const String &json, TelegramUpdate *upd);
bool parseUintArg(const String &args, uint16_t &value, uint16_t defaultValue, uint16_t minValue, uint16_t maxValue);
static bool isUnsignedInteger(const String &s);

static CommandResult cmdHelp(const String &args);
static CommandResult cmdStatus(const String &args);
static CommandResult cmdStats(const String &args);
static CommandResult cmdLog(const String &args);
static CommandResult cmdNotify(const String &args);
static CommandResult cmdQuiet(const String &args);
static CommandResult cmdReboot(const String &args);
static CommandResult cmdLed(const String &args);
static CommandResult cmdOta(const String &args);
static void doReboot();
static void doOta();

//=============================================================================
// Comandos
//=============================================================================

static const TelegramCommand COMMANDS[] = { 
   { "/h", cmdHelp },   { "/help", cmdHelp },     { "/s", cmdStatus }, { "/status", cmdStatus },
   { "/e", cmdStats },  { "/stats", cmdStats },   { "/l", cmdLog },    { "/log", cmdLog },
   { "/n", cmdNotify }, { "/notify", cmdNotify }, { "/q", cmdQuiet },  { "/quiet", cmdQuiet },
   { "/led", cmdLed },  { "/reboot", cmdReboot }, { "/ota", cmdOta } };

constexpr uint8_t NUM_COMMANDS = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

CommandResult telegramProcessCommand(const String &text) {
   String comando = text;
   String args;
   CommandResult result;

   int p = text.indexOf(' ');

   if (p >= 0) {
      comando = text.substring(0, p);
      args = text.substring(p + 1);
      args.trim();
   }

   for (uint8_t i = 0; i < NUM_COMMANDS; i++) {
      if (comando.equalsIgnoreCase(COMMANDS[i].comando)) {
         result = COMMANDS[i].handler(args);
         return result;
      }
   }
   result.message = "Comando desconhecido.\nDigite /help.";
   return result;
}

CommandResult cmdHelp(const String &) {

   CommandResult result;

   result.message = "==========================\n";
   result.message += "Monitora Links - Comandos\n";
   result.message += "==========================\n";
   result.message += "/status | <b>/s</b>\nStatus atual do sistema\n\n";
   result.message += "/stats | <b>/e</b>   [dias]\nEstatísticas\n\n";
   result.message += "/log | <b>/l</b>   [n]\nÚltimos eventos\n\n";
   result.message += "/notify | <b>/n</b>\nMostra a configuração das notificações\n\n";
   result.message += "/notify | <b>/n</b>   on|off\nAtiva/desativa notificações\n\n";
   result.message += "/quiet | <b>/q</b>\nConfiguração do período quieto (PQ)\n\n";
   result.message += "/quiet | <b>/q</b>   on|off\nAtiva/desativa PQ\n\n";
   result.message += "/quiet | <b>/q</b>   hh:mm hh:mm\nDefine PQ\n\n";
   result.message += "<b>/led</b>\nConfiguração do led\n\n";
   result.message += "<b>/led</b>   on|off|q[uiet]\nAtiva/desativa/desativa no PQ\n\n";
   result.message += "<b>/ota</b>\nAtualiza o firmware\n\n";
   result.message += "<b>/reboot</b>\nReinicia o monitor\n\n";

   return result;
}

CommandResult cmdStatus(const String &) {
   CommandResult result;

   result.message = buildStatus();
   result.message += buildSystemSummary();
   return result;
}

CommandResult cmdStats(const String &args) {
   uint16_t dias;
   CommandResult result;

   if (!parseUintArg(args, dias, 5, 1, 180)) {
      result.message = "Uso:\n/stats [dias]\ndias = 1-180";
   } else {
      result.message = buildStatistics(dias);
   }
   return result;
}

CommandResult cmdLog(const String &args) {
   uint16_t eventos;
   CommandResult result;

   if (!parseUintArg(args, eventos, 10, 1, 180)) {
      result.message = "Uso:\n/Log [eventos]\neventos = 1-180";
   } else {
      result.message = buildLog(eventos);
   }
   return result;
}

CommandResult cmdNotify(const String &args) {
   String arg1, arg2, arg3;
   int8_t totArgs;

   CommandResult result;

   totArgs = splitArgs(args, arg1, arg2, arg3);

   if (totArgs == 0) {
      result.message = buildInfoNotif();

   } else if (totArgs == 1 && arg1.equalsIgnoreCase("on")) {
      gConfig.notification.enabled = true;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\n🔔 Notificações ATIVADAS";

   } else if (totArgs == 1 && arg1.equalsIgnoreCase("off")) {
      gConfig.notification.enabled = false;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\n🔕 Notificações DESATIVADAS";

   } else {
      result.message = "⚠️ Comando inválido.\nUso:\n"
                       "/notify|/n\n"
                       "/notify|/n on|off\n";
   }
   return result;
}

CommandResult cmdQuiet(const String &args) {
   String arg1, arg2, arg3;
   int8_t totArgs;
   uint16_t min1, min2;

   CommandResult result;

   totArgs = splitArgs(args, arg1, arg2, arg3);

   if (totArgs == 0) {
      result.message += "\nPeríodo quieto:\n";
      result.message += (gConfig.notification.quietEnabled) ? "ATIVO - " : "INATIVO - ";
      result.message += formatTime(gConfig.notification.quietStart);
      result.message += " - ";
      result.message += formatTime(gConfig.notification.quietEnd);

   } else if (totArgs == 1 && arg1.equalsIgnoreCase("on")) {
      gConfig.notification.quietEnabled = true;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\n🔔 Período quieto ATIVADO\n" + formatTime(gConfig.notification.quietStart) + " - " + formatTime(gConfig.notification.quietEnd);

   } else if (totArgs == 1 && arg1.equalsIgnoreCase("off")) {
      gConfig.notification.quietEnabled = false;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\n🔕 Período quieto DESATIVADO";

   } else if (totArgs == 2 && parseTime(arg1, &min1) && parseTime(arg2, &min2)) {
      if (min1 == min2) {
         result.message = "\n⚠️ Horários inicial e final devem ser diferentes";
      } else {
         gConfig.notification.quietStart = min1;
         gConfig.notification.quietEnd = min2;
         gConfig.notification.quietEnabled = true;
         saveStorage(FILE_CONFIG, gConfig);
         result.message =
             "🔔 Período quieto:\nREDEFINIDO e ATIVADO\n" + formatTime(gConfig.notification.quietStart) + " - " + formatTime(gConfig.notification.quietEnd);
      }

   } else {
      result.message = "⚠️ Comando inválido.\nUso:\n"
                       "/quiet|/q\n"
                       "/quiet|/q on|off\n"
                       "/quiet|/q hh:mm hh:mm\n";
   }
   return result;
}

CommandResult cmdLed(const String &args) {
   CommandResult result;

   String arg1, arg2, arg3;
   int8_t totArgs;

   totArgs = splitArgs(args, arg1, arg2, arg3);

   if (totArgs == 0) {
      result.message = buildInfoLed();

   } else if (totArgs == 1 && arg1.equalsIgnoreCase("off")) {
      gConfig.notification.ledMode = LED_MODE_OFF;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\nAtividade dos leds:\n⚪ DESATIVADA";

   } else if (totArgs == 1 && arg1.equalsIgnoreCase("on")) {
      gConfig.notification.ledMode = LED_MODE_ON;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\nAtividade dos leds:\n🔵 ATIVADA";

   } else if (totArgs == 1 && (arg1.equalsIgnoreCase("q") || arg1.equalsIgnoreCase("quiet"))) {
      gConfig.notification.ledMode = LED_MODE_QUIET;
      saveStorage(FILE_CONFIG, gConfig);
      result.message = "\nAtividade do led:\n⚪🔵 DESATIVADA no PQ";

   } else {
      result.message = "⚠️ Comando inválido.\nUso:\n\n"
                       "/led\n"
                       "/led on|off|q[uiet]";
   }

   return result;
}

CommandResult cmdReboot(const String &) {
   CommandResult result;
   result.deferredFunction = doReboot;
   result.message = "🔄 Reiniciando o monitor...";
   return result;
}

CommandResult cmdOta(const String &) {
   CommandResult result;

   result.message = "⬇️ Iniciando atualização do firmware...";
   result.deferredFunction = doOta;

   return result;
}

void doReboot() {

   gRuntime.bootReason = BOOT_AFTER_REBOOT_COMMAND;
   gRuntime.rebootStartTime = now();
   saveStorage(FILE_RUNTIME, gRuntime);

   ESP.restart();
}

void doOta()
{
   String availableVersion;

   if (!getOtaVersion(OTA_VERSION_URL, availableVersion)) {
      telegramSendMessage("❌ Não foi possível verificar a versão disponível.");
      return;
   }

   int cmp = compareVersions(availableVersion.c_str(), FW_VERSION);

   if (cmp == 0) {
      telegramSendMessage(
         "✅ Firmware já está atualizado.\n"
         "Versão: " FW_VERSION);
      return;
   }

   if (cmp < 0) {
      String msg = "⚠️ A versão disponível (";
      msg += availableVersion.c_str();
      msg += ") é anterior à atual (";
      msg += FW_VERSION;
      msg += ").\n";
      telegramSendMessage(msg);
      return;
   }

   DBG("\nIniciando atualização OTA...\n");

   if (!updateOta(OTA_FIRMWARE_URL)) {
      DBG("\nFalha na atualização OTA...\n");
      telegramSendMessage("❌ Falha na atualização do firmware.");
      return;
   }

   gRuntime.rebootStartTime = now();
   gRuntime.bootReason = BOOT_AFTER_OTA;
   saveStorage(FILE_RUNTIME, gRuntime);

   ESP.restart();
}

//=============================================================================
// Inicialização
//=============================================================================

bool telegramInit() { return true; }

//=============================================================================
// Send Message
//=============================================================================

bool telegramSendMessage(const String &text) {

   String url = "https://" + String(TELEGRAM_HOST) + "/bot" + gPerfil->telegramConfig->telegramToken + "/sendMessage";

   // DBG("Entrou no telegramSendMessage. URL = \n%s\n", url.c_str());

   WiFiClientSecure client;
   client.setInsecure();
   HTTPClient http;

   if (!http.begin(client, url)) {
      DBG("Telegram: erro em http.begin()\n");
      return false;
   }

   http.addHeader("Content-Type", "application/x-www-form-urlencoded");

   String body = "chat_id=" + String(gPerfil->telegramConfig->telegramChatId) + "&parse_mode=HTML" + "&text=" + text;

   int code = http.POST(body);

   if (code == HTTP_CODE_OK) {
      DBG("Telegram: mensagem enviada\n");
   } else {
      DBG("Telegram... %s (%d)\n", http.errorToString(code).c_str(), code);
   }

   http.end();

   return (code == HTTP_CODE_OK);
}

//=============================================================================
// Get Updates
//=============================================================================

bool telegramGetUpdates(TelegramUpdate *upd) {

   String url = "https://" + String(TELEGRAM_HOST) + "/bot" + gPerfil->telegramConfig->telegramToken +
                "/getUpdates?offset=" + String(gRuntime.telegramUpdateId + 1) + "&limit=1";

   // DBG("Entrou no telegramGetUpdates. URL = \n%s\n", url.c_str());

   WiFiClientSecure client;
   client.setInsecure();
   HTTPClient http;

   if (!http.begin(client, url)) {
      DBG("Telegram: erro em http.begin()\n");
      return false;
   }

   uint32_t t = millis();
   // DBG("Vai fazer o http.GET...\n");

   int code = http.GET();

   // DBG("http.GET terminado: %lu ms\n", millis() - t);

   bool ok = false;

   if (code == HTTP_CODE_OK) {
      String json = http.getString();
      ok = telegramParseUpdate(json, upd);
   } else {
      DBG("Telegram... %s (%d)\n", http.errorToString(code).c_str(), code);
   }

   http.end();

   return ok;
}

//=============================================================================
// Parse Update
//=============================================================================

static bool telegramParseUpdate(const String &json, TelegramUpdate *upd) {
   int p, q;
   // update_id
   p = json.indexOf("\"update_id\":");
   if (p < 0)
      return false;

   p += 12;
   upd->updateId = strtoul(json.c_str() + p, nullptr, 10);

   // chat.id
   p = json.indexOf("\"chat\":");
   if (p < 0)
      return false;

   p = json.indexOf("\"id\":", p);
   if (p < 0)
      return false;

   p += 5;
   q = p;
   if (json[q] == '-')
      q++;

   while (q < json.length() && isDigit(json[q]))
      q++;

   upd->chatId = json.substring(p, q);

   // text
   p = json.indexOf("\"text\":\"");
   if (p < 0) {
      upd->text = "";
      return true; // Update válido, mas sem mensagem de texto.
   }

   p += 8;
   q = json.indexOf('"', p);
   if (q < 0)
      return false;

   upd->text = json.substring(p, q);

   return true;
}

uint8_t splitArgs(const String &args, String &arg1, String &arg2, String &arg3) {

   if (args.isEmpty())
      return 0;

   int totArgs = 1;
   int p1 = args.indexOf(' ');

   if (p1 < 0) {
      arg1 = args;
   } else {
      arg1 = args.substring(0, p1);
      int p2 = args.indexOf(' ', p1 + 1);
      if (p2 < 0) {
         arg2 = args.substring(p1 + 1);
         totArgs = 2;
      } else {
         arg2 = args.substring(p1 + 1, p2);
         arg3 = args.substring(p2 + 1);
         totArgs = 3;
      }
   }

   return totArgs;
}

bool parseUintArg(const String &args, uint16_t &value, uint16_t defaultValue, uint16_t minValue, uint16_t maxValue) {

   if (args.isEmpty()) {
      value = defaultValue;
   } else {
      if (!isUnsignedInteger(args)) {
         return false;
      }
      value = args.toInt();
      if (value < minValue || value > maxValue)
         return false;
   }

   return true;
}

bool parseTime(const String &str, uint16_t *minutes) {
   if (str.length() != 5)
      return false;
   if (str.charAt(2) != ':')
      return false;
   if (!isdigit(str.charAt(0)) || !isdigit(str.charAt(1)) || !isdigit(str.charAt(3)) || !isdigit(str.charAt(4)))
      return false;

   uint8_t h = (str.charAt(0) - '0') * 10 + str.charAt(1) - '0';

   uint8_t m = (str.charAt(3) - '0') * 10 + str.charAt(4) - '0';

   if (h > 23 || m > 59)
      return false;

   *minutes = h * 60 + m;

   return true;
}

static bool isUnsignedInteger(const String &s) {

   for (uint16_t i = 0; i < s.length(); i++) {
      if (!isDigit(s[i]))
         return false;
   }

   return true;
}
//=============================================================================
// Autorização
//=============================================================================

bool isAuthorizedChat(const String &chatId) {

   if (String(gPerfil->telegramConfig->telegramChatId) != chatId) {
      DBG("Telegram: chatId da origem não bate com o autorizado\n");
      DBG("chatId origem = %s\n", chatId.c_str());
      DBG("TELEGRAM_CHAT_ID = %s\n", String(gPerfil->telegramConfig->telegramChatId).c_str());
      return false;
   }
   return true;
}
