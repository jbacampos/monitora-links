#pragma once

#ifdef ESP32

void initLedTask();
void initTelegramTask();

void initRuntimeMutex();
void lockRuntime();
void unlockRuntime();

void openServiceWindow();
void closeServiceWindow();

#endif