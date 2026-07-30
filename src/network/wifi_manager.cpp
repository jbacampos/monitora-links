
#include "config/config.h"
#include "hardware/led.h"
#include "network/wifi_manager.h"
#include "network/ntp.h"
#include "config/platform.h"

//=============================================================================
// Auxiliares
//=============================================================================

static bool testInternet() {
   WiFiClient client;

   if (client.connect(IPAddress(8, 8, 8, 8), 53)) {
      client.stop();
      return true;
   }

   return false;
}

//=============================================================================
// Conexão Wi-Fi
//=============================================================================

bool connectWifi(const char* ssid, const char* password) {

   uint32_t t0 = millis();
   DBG("Início do connectWifi: %lu ms\n", millis() - t0);

   WiFi.mode(WIFI_STA);
   WiFi.setAutoReconnect(false);

#ifdef ESP8266
   WiFi.setSleepMode(WIFI_NONE_SLEEP);
#elif defined(ESP32)
   WiFi.setSleep(false);
#endif
   WiFi.begin(ssid, password);

   uint32_t start = millis();
   uint8_t cont = 0;

   while (WiFi.status() != WL_CONNECTED) {
      cont++;
      ledsUpdate();
      delay(400);
      if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
         DBG("Timeout\n");
         return false;
      }
   }
   DBG("Testes no while: %u\n", cont);

   // DBG("Status final: %d\n", WiFi.status());
   // DBG("Canal: %d\n", WiFi.channel());
   // DBG("BSSID: %s\n", WiFi.BSSIDstr().c_str());
   // DBG("IP   : %s\n", WiFi.localIP().toString().c_str());
   // DBG("RSSI : %d dBm\n", WiFi.RSSI());

   // String mac = WiFi.macAddress();
   // DBG("MAC: %s\n", mac.c_str());

   return true;
}

// bool ssidFound(const char* ssid) {
//    int total = WiFi.scanNetworks();
// uint32_t t0 = millis();
//    for (int i = 0; i < total; i++) {
//       if (WiFi.SSID(i) == ssid)
//       return true;
//    }
//    return false;
// }

void disconnectWifi() {
   WiFi.disconnect(true);
   // delay(200);

   // WiFi.mode(WIFI_OFF);
   // delay(500);
}

//=============================================================================
// Diagnóstico
//=============================================================================

LinkStatus testConnection(const char* ssid, const char* password, int16_t* rssi, uint8_t retries) {
   *rssi = 0;
   LinkStatus status = LINK_WIFI_FAIL;
   for (uint8_t tentativa = 0; tentativa < retries; tentativa++) {
      if (!connectWifi(ssid, password)) {
         setColor(LED_RED);
         status = LINK_WIFI_FAIL;
         DBG("%s link fail. Tentativa %d\n", ssid, tentativa + 1);
      } else {
         *rssi = WiFi.RSSI();
         bool internetOk = testInternet();

         if (internetOk) {
            setColor(LED_GREEN);
            status = LINK_ONLINE;
         } else {
            setColor(LED_RED);
            status = LINK_INTERNET_FAIL;
         }

         if (internetOk && !clockIsValid())
            syncClock();
         status = internetOk ? LINK_ONLINE : LINK_INTERNET_FAIL;
      }

      if (status == LINK_ONLINE)
         break;

      if (tentativa < retries - 1) {
         delay(RETRY_DELAY_MS);
      }
   }

   return status;
}
