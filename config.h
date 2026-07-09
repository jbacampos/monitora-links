#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "secrets.h"

/*********************************************************************
 * VERSÃO
 *********************************************************************/

#define FW_VERSION "1.0"
#define MAGIC_NUMBER 0x56434D4F

// Incrementar sempre que PersistState mudar:
#define STATE_VERSION 5 

/*********************************************************************
 * MODOS
 *********************************************************************/

#define DEV_MODE true
#define EM_POA true
#define ENABLE_SLEEP false 
      // Não usado por ora. Serve apenas para economizar energia, caso aalimentação seja por bateria

/*********************************************************************
 * LED
 *********************************************************************/

constexpr uint8_t LED_RED_PIN = D5;
constexpr uint8_t LED_GREEN_PIN = D6;
constexpr uint8_t LED_BLUE_PIN = D7;
constexpr uint16_t LED_BLINK_PERIOD_MS = 300;
constexpr uint16_t LED_STATUS_HOLD_MS = 5000;

enum LedMode : uint8_t { LED_MODE_OFF = 0, LED_MODE_ON, LED_MODE_QUIET };

/*********************************************************************
 * NOTIFICAÇÕES
 *********************************************************************/

#define NOTIFICATIONS_ENABLED_DEFAULT true
#define QUIET_HOURS_ENABLED_DEFAULT true
#define QUIET_HOURS_START_DEFAULT (22 * 60)
#define QUIET_HOURS_END_DEFAULT (7 * 60)
#define LED_MODE_DEFAULT LED_MODE_QUIET

/*********************************************************************
 * ESTATÍSTICAS
 *********************************************************************/

#define MAX_EVENTS 256

/*********************************************************************
 * ARQUIVOS
 *********************************************************************/

#define FILE_STATE "/state.bin"
#define FILE_EVENTS "/events.bin"
#define FILE_QUEUE "/queue.bin"

/******************************************************************************
 * NTP
 ******************************************************************************/

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "a.st1.ntp.br"
#define NTP_SERVER_3 "time.google.com"

#define GMT_OFFSET_SEC (-3 * 3600)
#define DAYLIGHT_OFFSET_SEC 0
#define SECONDS_PER_DAY 86400UL

/******************************************************************************
 * TELEGRAM
 ******************************************************************************/

// As duas constantes abaiso são definidos em "secrets.h"
// #define TELEGRAM_BOT_TOKEN "9999999999:paosdirjgoirejgpseoitjgsopjgbspod"
// #define TELEGRAM_CHAT_ID "-23546529854"

#define TELEGRAM_HOST "api.telegram.org"
#define TELEGRAM_PORT 443
#define MAX_PENDING_NOTIFICATIONS 4

/*********************************************************************
 * MONITORAMENTO
 *********************************************************************/

constexpr uint8_t LINK_TEST_RETRIES = 3;
constexpr uint16_t RETRY_DELAY_MS = 300;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000;

/*********************************************************************
 * DEBUG
 *********************************************************************/

#if DEV_MODE
#define DBG(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(...)
#endif

#endif