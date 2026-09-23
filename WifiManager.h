#pragma once

#include <Arduino.h>
#include "Config.h"

// Unified WiFi bring-up for OTA, Drive upload, and mail.
// Caller sets wifiActive=true before calling and false after.

bool wifiBringUp(unsigned long perAttemptTimeoutMs = WIFI_ATTEMPT_TIMEOUT_MS);
void wifiBringDown();
bool wifiPingTest();

// V3.31: message shown on OLED during WiFi
extern char wifiMessage[24];