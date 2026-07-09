#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"

//=============================================================================
// Conexão Wi-Fi
//=============================================================================

bool connectWifi(const char *ssid, const char *password);

void disconnectWifi();

//=============================================================================
// Diagnóstico
//=============================================================================

LinkStatus testConnection(const char *ssid, const char *password, int16_t *rssi,
                          uint8_t retries);

#endif