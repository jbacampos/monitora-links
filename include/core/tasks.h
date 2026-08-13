#pragma once

#include <stdint.h>
#include <freertos/FreeRTOS.h>

enum TelegramAction {
    TELEGRAM_ACTION_NONE,
    TELEGRAM_ACTION_REBOOT,
    TELEGRAM_ACTION_OTA
};

constexpr uint8_t COMMAND_TEXT_SIZE = 64;
struct MonitorCommand {
    char text[COMMAND_TEXT_SIZE];
};    

constexpr uint16_t TELEGRAM_MESSAGE_SIZE = 4096;
struct TelegramMessage {    
    char text[TELEGRAM_MESSAGE_SIZE];
    TelegramAction action;
};

bool queueMonitorCommand(const char *text);
bool getMonitorCommand(MonitorCommand &command);

bool queueTelegramMessage(const char *text, TelegramAction action = TELEGRAM_ACTION_NONE);
bool getTelegramMessage(TelegramMessage &message);

void initLedTask();
void initTelegramTask();

void initRuntimeMutex();
void lockRuntime();
void unlockRuntime();

void openServiceWindow();
void closeServiceWindow();