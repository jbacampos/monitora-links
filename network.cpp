#include <ESP8266WiFi.h>

#include "config.h"
#include "led.h"
#include "network.h"
#include "ntp.h"

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

bool connectWifi(const char *ssid, const char *password) {
   WiFi.mode(WIFI_STA);

   WiFi.begin(ssid, password);

   uint32_t start = millis();

   while (WiFi.status() != WL_CONNECTED) {

      ledsUpdate();

      delay(20);

      if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
         DBG("Timeout\n");
         return false;
      }
   }

   DBG("IP   : %s\n", WiFi.localIP().toString().c_str());
   DBG("RSSI : %d dBm\n", WiFi.RSSI());

   return true;
}

void disconnectWifi() {
   WiFi.disconnect(true);
   //delay(200);

   WiFi.mode(WIFI_OFF);
   // delay(500);
}

//=============================================================================
// Diagnóstico
//=============================================================================

LinkStatus testConnection(const char *ssid, const char *password, int16_t *rssi,
                          uint8_t retries) {
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
