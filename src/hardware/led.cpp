#include "hardware/led.h"

#include "config/config.h"

// -----------------------------------------------------------------------------
// Estado interno
// -----------------------------------------------------------------------------

enum LedActivity : uint8_t { LED_IDLE, LED_BUSY };

static LedActivity gMode = LED_IDLE;
static LedStatus gStatus = LED_ALL_UP;

static bool blinkOn = false;
static uint32_t lastBlink = 0;

static LedColor flashColor = LED_BLANK;
static uint32_t flashUntil = 0;

// -----------------------------------------------------------------------------
// Funções auxiliares
// -----------------------------------------------------------------------------

void ledFlash(LedColor color, uint32_t durationMs) {
   flashColor = color;
   flashUntil = millis() + durationMs;
}

void ledBusy() {
   gMode = LED_BUSY;
   blinkOn = false;
   lastBlink = 0;
}

void ledIdle() {
   gMode = LED_IDLE;
   blinkOn = false;
}

static bool systemReady = false;

void ledSystemReady() {
   systemReady = true;
}
void setColor(LedColor color) {

   uint16_t r = 0;
   uint16_t g = 0;
   uint16_t b = 0;

   switch (color) {
   case LED_GREEN:
      g = 50;
      break;
   case LED_BLUE:
      b = 50; // Azul menos intenso
      break;
   case LED_YELLOW:
   #ifdef ESP32
      r = 600;
      g = 15; // Ajustado visualmente
   #else // ESP8266:
      r = 500;
      g = 30; // Ajustado visualmente
   #endif
      break;
   case LED_RED:
      r = 1023;
      break;
   case LED_MAGENTA:
      r = 500;
      b = 30;
      break;
   case LED_CYAN:
      g = 100;
      b = 50;
      break;
   case LED_WHITE:
      r = 500;
      g = 200;
      b = 200;
      break;

   case LED_BLANK:

   default:
      break;
   }

   analogWrite(LED_RED_PIN, r);
   analogWrite(LED_GREEN_PIN, g);
   analogWrite(LED_BLUE_PIN, b);
}

static void showStatus() {

   switch (gStatus) {
   case LED_ALL_UP:
      setColor(LED_GREEN); // Verde
      break;
   case LED_PARTIAL_DOWN:
      setColor(LED_YELLOW); // Amarelo
      break;
   case LED_ALL_DOWN:
      setColor(LED_RED); // Vermelho
      break;
   }
}


bool ledEnabled() {

   switch (gConfig.notification.ledMode) {

   case LED_MODE_OFF:
      return false;

   case LED_MODE_ON:
      return true;

   case LED_MODE_QUIET:
      return !inQuietHours();
   }

   return true;
}

// -----------------------------------------------------------------------------
// Interface pública
// -----------------------------------------------------------------------------

void ledInit() {

   pinMode(LED_RED_PIN, OUTPUT);
   pinMode(LED_GREEN_PIN, OUTPUT);
   pinMode(LED_BLUE_PIN, OUTPUT);

   setColor(LED_BLANK);

#ifdef ESP8266   

// Autoteste
   int16_t espera = 250;
   int8_t i;
   for (int i = 0; i < 5; i++) {
      setColor(LED_BLUE);
      delay(espera);
   }
   for (int i = 0; i < 5; i++) {
      setColor(LED_GREEN);
      delay(espera);
   }
   for (int i = 0; i < 5; i++) {
      setColor(LED_YELLOW);
      delay(espera);
   }
   for (int i = 0; i < 5; i++) {
      setColor(LED_RED);
      delay(espera);
   }
   
   setColor(LED_BLANK);

#endif
}

void ledBeginCycle() {

   gMode = LED_BUSY;
   blinkOn = true;
   lastBlink = 0;
}

void ledEndCycle(LedStatus status) {

   gStatus = status;
   gMode = LED_IDLE;
}

void ledUpdate() {

   if (systemReady && !ledEnabled()) {
      blinkOn = false;
      setColor(LED_BLANK);
      return;
   }

   uint32_t now = millis();

   // Flash temporário tem prioridade
   if (flashUntil != 0) {
      if ((int32_t)(flashUntil - now) > 0) {
         setColor(flashColor);
         return;
      }

      flashUntil = 0;
   }

   // Ciclo terminado: mostra estado dos links
   if (gMode == LED_IDLE) {
      showStatus();
      return;
   }

   // Ciclo em andamento: pisca azul
   if (now - lastBlink < LED_BLINK_PERIOD_MS)
      return;

   lastBlink = now;
   blinkOn = !blinkOn;

   setColor(blinkOn ? LED_BLUE : LED_BLANK);
}
