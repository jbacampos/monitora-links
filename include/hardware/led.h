#ifndef LED_H
#define LED_H

#include <Arduino.h>

#include "network/notify.h"
#include "core/types.h"

enum LedStatus : uint8_t { LED_ALL_UP, LED_PARTIAL_DOWN, LED_ALL_DOWN };

enum LedColor : uint8_t {
   LED_BLANK,
   LED_RED,
   LED_GREEN,
   LED_BLUE,
   LED_YELLOW,
   LED_MAGENTA,
   LED_CYAN,
   LED_WHITE
};

void setColor(LedColor color);

bool ledEnabled();

void ledInit();

void ledBeginCycle();

void ledEndCycle(LedStatus status);

void ledSystemReady();

void ledUpdate();

void ledFlash(LedColor color, uint32_t durationMs);
void ledBusy();
void ledIdle();
void ledStartup();   

#endif