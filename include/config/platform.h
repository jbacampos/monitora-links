#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef ESP8266

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LittleFS.h>
#include <user_interface.h>

#elif defined(ESP32)

#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>

#endif

#endif