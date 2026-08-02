#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

bool getOtaVersion(const char* url, String& version);
int compareVersions(const char* a, const char* b);
bool updateOta(const char* url);


#endif