
#include "network/wifi_manager.h"
#include "config/config.h"
#include "config/platform.h"
#include "hardware/led.h"
#include "network/ntp.h"

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

volatile int wifiDisconnectReason = 0;

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
   if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifiDisconnectReason = info.wifi_sta_disconnected.reason;
   }
}

//=============================================================================
// Conexão Wi-Fi
//=============================================================================

bool connectWifi(const char *ssid, const char *password) {

   uint32_t t0 = millis();
   // DBG("Início do connectWifi: %lu ms\n", millis() - t0);

   WiFi.mode(WIFI_STA);
   WiFi.setAutoReconnect(false);

   WiFi.setSleep(false);

   // WiFi.disconnect();
   // delay(100);
   wifiDisconnectReason = 0;

   WiFi.begin(ssid, password);

   uint32_t start = millis();
   uint8_t cont = 1;

   while (WiFi.status() != WL_CONNECTED) {
      // DBG("Tentativa de conexão Wi-Fi %u: status = %d\n", cont, WiFi.status());
      cont++;

      if (wifiDisconnectReason != 0 && wifiDisconnectReason != 8) {
         DBG("Falha Wi-Fi. reason = %d\n", wifiDisconnectReason);
         return false;
      }
      ledUpdate();
      delay(RETRY_DELAY_MS);

      if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
         DBG("Timeout na tentativa de conexão\n");
         return false;
      }
   }

   DBG("Conexão bem sucedida. Status = %d, testes de wifi.status: %lu\n", WiFi.status(), cont);

   // DBG("Status final: %d\n", WiFi.status());
   // DBG("Canal: %d\n", WiFi.channel());
   // DBG("BSSID: %s\n", WiFi.BSSIDstr().c_str());
   // DBG("IP   : %s\n", WiFi.localIP().toString().c_str());
   // DBG("RSSI : %d dBm\n", WiFi.RSSI());

   // String mac = WiFi.macAddress();
   // DBG("MAC: %s\n", mac.c_str());

   return true;
}

void disconnectWifi() {
   WiFi.disconnect(true);
   // delay(200);

   // WiFi.mode(WIFI_OFF);
   // delay(500);
}

bool connectToOnlineLink(const LinkStatus status[]) {

   for (uint8_t i = 0; i < gPerfil->numLinks; i++) {

      if (status[i] != LINK_ONLINE)
         continue;

      // DBG("\nConectando ao link %s para serviços...\n", gPerfil->links[i].nome);

      if (connectWifi(gPerfil->links[i].ssid,
                      gPerfil->links[i].senha)) {

         DBG("Link %s disponível para serviços\n", gPerfil->links[i].nome);

         return true;
      }
   }

   DBG("Nenhum link disponível para serviços\n");
   return false;
}

//=============================================================================
// Diagnóstico
//=============================================================================

LinkStatus testConnection(const char *ssid, const char *password, int16_t *rssi, uint8_t retries) {
   *rssi = 0;
   LinkStatus status = LINK_WIFI_FAIL;
   for (uint8_t tentativa = 0; tentativa < retries; tentativa++) {
      if (!connectWifi(ssid, password)) {
         ledFlash(LED_MAGENTA, 150);         
         status = LINK_WIFI_FAIL;
         DBG("%s link fail. Tentativa %d\n", ssid, tentativa + 1);
      } else {
         *rssi = WiFi.RSSI();
         bool internetOk = testInternet();

         if (internetOk) {
            ledFlash(LED_GREEN, 150);         
            status = LINK_ONLINE;
         } else {
            ledFlash(LED_MAGENTA, 150);         
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
