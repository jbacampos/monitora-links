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
   void (*deferredFunction)() = nullptr;
} CommandResult;


CommandResult cmdStatus(const String &args);


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

CommandResult telegramProcessCommand(const String &text);

uint8_t splitArgs(const String &args, String &arg1, String &arg2, String &arg3);
bool parseTime(const String &str, uint16_t *minutes);

//=============================================================================
// Autorização
//=============================================================================

bool isAuthorizedChat(const String &chatId);

#endif