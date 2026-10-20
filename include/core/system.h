#ifndef SYSTEM_H
#define SYSTEM_H

#include "core/types.h"

//=============================================================================
// Inicialização do sistema
//=============================================================================

void initSystem();

//=============================================================================
// Estado do sistema
//=============================================================================

BootReason getBootReason();

// Motivo bruto do último reset, conforme esp_reset_reason().
// Capturado uma única vez na inicialização; 0 (ESP_RST_UNKNOWN) fora do ESP32.
uint8_t getResetReasonRaw();

#endif