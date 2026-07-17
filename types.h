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

#include "config.h"
#include "secrets.h"

//=============================================================================
// Tipos básicos
//=============================================================================

typedef uint8_t LinkId;

//=============================================================================
// Configuração dos links monitorados
//=============================================================================

typedef struct {
   const char *nome;
   const char *ssid;
   const char *senha;
} LinkConfig;

// Para alterar ou incluir links a serem monitorados:
// 1) altere o valor de NUM_LINKS
// 2) aumente ou diminua o array LINKS
// 3) configure as constantes LINK_x, SSID_x e PASSWORD_x em "secrets.h"

#if EM_POA
constexpr uint8_t NUM_LINKS = 3;
constexpr LinkConfig LINKS[NUM_LINKS] = {
    {LINK_1, SSID_1, PASSWORD_1},
    {LINK_2, SSID_2, PASSWORD_2},
    {LINK_3, SSID_3, PASSWORD_3},
};

#else

constexpr uint8_t NUM_LINKS = 2;
constexpr LinkConfig LINKS[NUM_LINKS] = {
    {LINK_1, SSID_1, PASSWORD_1},
    {LINK_2, SSID_2, PASSWORD_2},
};

#endif

#define EmojiOnline  "🟢"
#define EmojiOffline "🔴"


//=============================================================================
// Enumerações
//=============================================================================

typedef enum : uint8_t {
   LINK_ONLINE,
   LINK_WIFI_FAIL,
   LINK_INTERNET_FAIL
} LinkStatus;

typedef enum : uint8_t {
   BOOT_POWERON,
   BOOT_DEEPSLEEP,
   BOOT_EXTERNAL,
   BOOT_WATCHDOG,
   BOOT_SOFTWARE,
   BOOT_UNKNOWN
} BootReason;

enum NotificationType : uint8_t { NOTIFY_DOWN, NOTIFY_UP };

//=============================================================================
// Estruturas
//=============================================================================

// Estado persistente de um link monitorado.
typedef struct {
   time_t ultimaMudanca;
   time_t inicioFalha;
   time_t ultimoTeste;
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

} Evento;

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
   time_t ultimoHorarioValido;
   uint32_t magic;
   uint32_t bootCount;
   uint32_t cicloCount;
   uint32_t watchdogCount;
   uint32_t ciclosSemNTP;
   uint32_t proximoEvento;
   uint32_t telegramUpdateId;

uint32_t saveCount;

   uint16_t version;
   uint16_t primeiroEvento;
   uint16_t numeroEventos;


   Evento eventos[MAX_EVENTS];
   LinkState links[NUM_LINKS];
   PendingNotification pendingNotifications[MAX_PENDING_NOTIFICATIONS];


} PersistState;

//=============================================================================
// Interface pública
//=============================================================================

extern PersistState gState;

const char *linkStatusName(LinkStatus status);
const char *linkStatusDescription(LinkStatus status);

#endif