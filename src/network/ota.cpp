//=============================================================================
// ota.cpp
//
// Atualização OTA do firmware por HTTP/HTTPS.
// Compatível com ESP32 e ESP8266.
//
// A função updateOta():
//   - baixa o firmware indicado pela URL;
//   - grava o firmware na área destinada à atualização OTA;
//   - verifica se a atualização foi concluída;
//   - retorna true em caso de sucesso.
//
// O reboot e as notificações são responsabilidade do chamador.
//=============================================================================

#include "network/ota.h"

#include <ArduinoJson.h>
#include "config/config.h"

#ifdef ESP32
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

#elif defined(ESP8266)
#include <ESP8266HTTPClient.h>
#include <Updater.h>
#include <WiFiClientSecureBearSSL.h>
#endif

bool updateOta(const char *url) {
   DBG("\n==========================\n");
   DBG("Atualização OTA\n");
   DBG("==========================\n");
   DBG("URL: %s\n", url);

#ifdef ESP32
   WiFiClientSecure client;
#elif defined(ESP8266)
   BearSSL::WiFiClientSecure client;
#endif

   // Primeira implementação:
   // usa HTTPS, mas não valida o certificado do servidor.
   client.setInsecure();

   HTTPClient http;

   DBG("Conectando ao servidor...\n");

   if (!http.begin(client, url)) {
      DBG("OTA: erro em http.begin()\n");
      return false;
   }

   // GitHub pode redirecionar o download do asset.
   http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

   DBG("Baixando firmware...\n");
   
   http.addHeader("User-Agent", "MonitoraLinks-ESP32");
   int httpCode = http.GET();

   DBG("HTTP code: %d\n", httpCode);

   if (httpCode != HTTP_CODE_OK) {
      DBG("OTA: erro HTTP: %d\n", httpCode);
      DBG("OTA: %s\n", http.errorToString(httpCode).c_str());

      http.end();
      return false;
   }

   int contentLength = http.getSize();

   DBG("Tamanho do firmware: %d bytes\n", contentLength);

   if (contentLength <= 0) {
      DBG("OTA: tamanho do firmware inválido.\n");
      http.end();
      return false;
   }

   if (!Update.begin(contentLength)) {
      DBG("OTA: não foi possível iniciar a atualização.\n");
      DBG("OTA: erro Update = %u\n", Update.getError());

      http.end();
      return false;
   }

   WiFiClient *stream = http.getStreamPtr();

   DBG("Gravando firmware...\n");

   size_t written = Update.writeStream(*stream);

   DBG("Bytes gravados: %u / %u\n", (unsigned)written, (unsigned)contentLength);

   if (written != (size_t)contentLength) {
      DBG("OTA: firmware recebido incompleto.\n");
#ifdef ESP32
      Update.abort();
#endif
      http.end();
      return false;
   }    

   if (!Update.end()) {
      DBG("OTA: erro ao finalizar atualização.\n");
      DBG("OTA: erro Update = %u\n", Update.getError());

      http.end();
      return false;
   }

   if (!Update.isFinished()) {
      DBG("OTA: atualização não foi concluída.\n");

      http.end();
      return false;
   }

   http.end();

   DBG("OTA concluído com sucesso.\n");

   return true;
}

bool getOtaVersion(const char* url, String& version)
{
   WiFiClientSecure client;
   client.setInsecure();

   HTTPClient http;

   if (!http.begin(client, url)) {
      DBG("OTA: erro em http.begin() ao consultar versão.\n");
      return false;
   }

   http.addHeader("User-Agent", "MonitoraLinks");

   int code = http.GET();

   if (code != HTTP_CODE_OK) {
      DBG("OTA: erro HTTP ao consultar versão: %d\n", code);
      http.end();
      return false;
   }

   String json = http.getString();

   JsonDocument doc;

   DeserializationError error = deserializeJson(doc, json);

   if (error) {
      DBG("OTA: erro ao interpretar JSON: %s\n", error.c_str());
      http.end();
      return false;
   }

   const char* tag = doc["tag_name"];

   if (tag == nullptr) {
      DBG("OTA: tag_name inexistente na resposta.\n");
      http.end();
      return false;
   }

   version = tag;

   // Aceita tags no formato v1.2.3 ou 1.2.3
   if (version.startsWith("v") || version.startsWith("V"))
      version.remove(0, 1);

   version.trim();

   DBG("Versão atual: %s\n", FW_VERSION);
   DBG("Versão disponível: %s\n", version.c_str());

   http.end();

   return true;
}

int compareVersions(const char* a, const char* b)
{
   unsigned int majorA = 0, minorA = 0, patchA = 0;
   unsigned int majorB = 0, minorB = 0, patchB = 0;

   if (sscanf(a, "%u.%u.%u", &majorA, &minorA, &patchA) != 3) {
      DBG("OTA: versão inválida: %s\n", a);
      return 0;
   }

   if (sscanf(b, "%u.%u.%u", &majorB, &minorB, &patchB) != 3) {
      DBG("OTA: versão inválida: %s\n", b);
      return 0;
   }

   if (majorA != majorB)
      return (majorA > majorB) ? 1 : -1;

   if (minorA != minorB)
      return (minorA > minorB) ? 1 : -1;

   if (patchA != patchB)
      return (patchA > patchB) ? 1 : -1;

   return 0;
}