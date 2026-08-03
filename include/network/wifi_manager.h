#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "core/types.h"
#include <WiFi.h>

//=============================================================================
// Conexão Wi-Fi
//=============================================================================

bool connectWifi(const char *ssid, const char *password);

bool ssidFound(const char* ssid);

void disconnectWifi();

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);

//=============================================================================
// Diagnóstico
//=============================================================================

LinkStatus testConnection(const char *ssid, const char *password, int16_t *rssi,
                          uint8_t retries);

#endif