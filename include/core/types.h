#ifndef TYPES_H
#define TYPES_H

//=============================================================================
// types.h
//
// Tipos, constantes e estruturas compartilhados por todo o projeto.
//
// Este módulo define o modelo de dados utilizado pelo Monitor Links,
// centralizando:
//
//   • Configuração dos links monitorados.
//   • Enumerações.
//   • Estruturas persistentes.
//   • Estruturas auxiliares.
//   • Funções de conversão.
//
//=============================================================================

#include <Arduino.h>
#include <time.h>

#include "config/secrets.h"
#include "core/constants.h"


//=============================================================================
// Tipos básicos
//=============================================================================

typedef uint8_t LinkId;

//=============================================================================
// Configuração dos perfis de monitoração
//=============================================================================

typedef struct {
   const char* nome; // nome da rede, como será exibida nos logs e notificações
   const char* ssid;
   const char* senha;
   bool identificaLocal;
} LinkConfig;

typedef struct {
   const char* telegramToken;
   const char* telegramChatId;
} TelegramConfig;

typedef struct {
   const char* nome; // nome do perfil, para exibição no debug
   uint8_t numLinks;
   const LinkConfig* links;
   const TelegramConfig* telegramConfig;
} Perfil;


//=============================================================================
// Enumerações
//=============================================================================

typedef enum : uint8_t { LINK_ONLINE, LINK_WIFI_FAIL, LINK_INTERNET_FAIL, LINK_UNKNOWN } LinkStatus;

typedef enum : uint8_t {
   BOOT_POWERON,
   BOOT_DEEPSLEEP,
   BOOT_EXTERNAL,
   BOOT_WATCHDOG,
   BOOT_SOFTWARE,
   BOOT_UNKNOWN
} BootReason;

enum NotificationType : uint8_t { NOTIFY_DOWN, NOTIFY_UP };

enum LedMode : uint8_t {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_QUIET
};

//=============================================================================
// Estruturas
//=============================================================================

// Estado persistente de um link monitorado.
typedef struct {
   time_t ultimaMudanca;
   time_t inicioFalha;
   uint32_t eventoAtual;
   uint32_t totalFalhas;
   uint32_t totalTestes;
   uint32_t testesFalhos;
   uint32_t tempoFalhaAcumulado;

   int16_t ultimoRSSI;

   LinkStatus status;
   LinkStatus motivoFalha;
   bool downNotificationSent;
} LinkState;

// Evento registrado no histórico.
typedef struct {
   time_t inicio;
   time_t fim;
   uint32_t id;
   uint32_t duracaoSeg;
   int16_t rssi;
   LinkId link;
   LinkStatus motivo;
   bool enviadoTelegram;

} Event;

// Notificação pendente de envio.
typedef struct {
   time_t inicio;
   time_t fim;
   uint32_t evento;
   uint32_t duracao;
   int16_t rssi;
   NotificationType tipo;
   LinkId link;
   LinkStatus motivo;
   bool pending;

} PendingNotification;

// Estatísticas calculadas para um link.
typedef struct {
   uint32_t downtime24h;
   uint32_t downtime7d;
   uint32_t downtime30d;
   uint32_t downtime180d;
   uint32_t totalDowntime;
   uint32_t eventCount;
} LinkStatistics;

typedef struct {
   uint16_t quietStart;
   uint16_t quietEnd;
   LedMode ledMode;
   bool enabled;
   bool quietEnabled;

} NotificationSettings;

// Estado persistente do sistema.
typedef struct {
   NotificationSettings notification;
   uint32_t magic;
   uint32_t bootCount;
   uint32_t watchdogCount;
   uint32_t lastEventId;
   uint32_t telegramUpdateId;
   uint32_t saveCount;
   uint16_t version;
   uint16_t firstEventId;
   uint16_t eventCounter;
   Event eventos[MAX_EVENTS];
   LinkState links[MAX_LINKS];
   PendingNotification pendingNotifications[MAX_PENDING_NOTIFICATIONS];

} PersistState;

struct StorageHeader {
   uint32_t magic;
   uint16_t version;
};

struct ConfigData {
   StorageHeader header;
   NotificationSettings notification;
};

struct RuntimeData {
   StorageHeader header;
   uint32_t bootCount;
   uint32_t watchdogCount;
   uint32_t saveCount;
   uint32_t telegramUpdateId;

   LinkState links[MAX_LINKS];
   PendingNotification pendingNotifications[MAX_PENDING_NOTIFICATIONS];
};

struct EventsData {
   StorageHeader header;

   uint32_t lastEventId;

   uint16_t firstEventId;
   uint16_t eventCounter;

   Event events[MAX_EVENTS];
};

//=============================================================================
// Interface pública
//=============================================================================

extern ConfigData gConfig;
extern RuntimeData gRuntime;
extern EventsData gEvents;

extern const Perfil* gPerfil;

const char* linkStatusDescription(LinkStatus status);

#endif