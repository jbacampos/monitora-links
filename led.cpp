#include "led.h"

#include "config.h"

// -----------------------------------------------------------------------------
// Estado interno
// -----------------------------------------------------------------------------

enum LedActivity : uint8_t { LED_IDLE, LED_BUSY };

static LedActivity gMode = LED_IDLE;
static LedStatus gStatus = LED_ALL_UP;

static bool blinkOn = false;
static uint32_t lastBlink = 0;

// -----------------------------------------------------------------------------
// Funções auxiliares
// -----------------------------------------------------------------------------

void setColor(LedColor color) {

   uint16_t r = 0;
   uint16_t g = 0;
   uint16_t b = 0;

   if (!ledsEnabled())
      color = LED_BLANK;

   switch (color) {
   case LED_GREEN:
      g = 50;
      break;
   case LED_BLUE:
      b = 50; // Azul menos intenso
      break;
   case LED_YELLOW:
      r = 500;
      g = 30; // Ajustado visualmente
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

bool ledsEnabled() {

   switch (gState.notification.ledMode) {

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

void ledsInit() {

   pinMode(LED_RED_PIN, OUTPUT);
   pinMode(LED_GREEN_PIN, OUTPUT);
   pinMode(LED_BLUE_PIN, OUTPUT);

   setColor(LED_BLANK);

   // Autoteste
   int16_t espera = 250;
   setColor(LED_BLUE);
   delay(espera);
   setColor(LED_GREEN);
   delay(espera);
   setColor(LED_RED);
   delay(espera);
   setColor(LED_YELLOW);
   delay(espera);
   setColor(LED_MAGENTA);
   delay(espera);
   setColor(LED_CYAN);
   delay(espera);
   setColor(LED_WHITE);
   delay(espera);

   setColor(LED_BLANK);
}

void ledsBeginCycle() {

   gMode = LED_BUSY;
   blinkOn = true;
   lastBlink = millis();
   setColor(LED_BLUE);
}

void ledsEndCycle(LedStatus status) {

   gStatus = status;
   gMode = LED_IDLE;

   showStatus();
}

void ledsUpdate() {

   if (gMode != LED_BUSY)
      return;

   if (millis() - lastBlink < LED_BLINK_PERIOD_MS)
      return;

   lastBlink = millis();
   blinkOn = !blinkOn;

   if (blinkOn) {
      setColor(LED_BLUE);
   } else {
      setColor(LED_BLANK);
   }
}