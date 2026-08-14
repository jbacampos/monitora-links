#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <Arduino.h>

//=============================================================================
// Estruturas
//=============================================================================

typedef struct {
   uint32_t updateId;
   String chatId;
   String text;
} TelegramUpdate;

extern TelegramUpdate gUpdate;

typedef struct {
   String message;
} CommandResult;


CommandResult cmdHelp(const String &args);
CommandResult cmdStatus(const String &args);
CommandResult cmdStats(const String &args);
CommandResult cmdLog(const String &args);
CommandResult cmdNotify(const String &args);
CommandResult cmdQuiet(const String &args);
CommandResult cmdLed(const String &args);
void doReboot();
void doOta();


//=============================================================================
// Inicialização
//=============================================================================

bool telegramInit();

//=============================================================================
// Comunicação
//=============================================================================

bool telegramSendMessage(const String &text);

bool telegramGetUpdates(TelegramUpdate *upd);

//=============================================================================
// Processamento
//=============================================================================

uint8_t splitArgs(const String &args, String &arg1, String &arg2, String &arg3);
bool parseTime(const String &str, uint16_t *minutes);

//=============================================================================
// Autorização
//=============================================================================

bool isAuthorizedChat(const String &chatId);

#endif