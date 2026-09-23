#pragma once

#include <Arduino.h>

// Core — send any email. Returns true on successful SMTP send.
bool sendMail(const String& subject, const String& body);

// Convenience wrappers — build formatted content, call sendMail()
bool sendLowBatteryAlert(float voltage, float threshold);
bool sendRideSummary(uint32_t startEpoch, uint32_t durationSecs,
                     float preVolt, float avgVolt, float minVolt, float maxVolt,
                     int8_t minTemp, int8_t maxTemp, uint8_t flags);
bool sendWeeklySummary(const String& text);

// Rate-limiting / diagnostics
uint32_t mailLastSentEpoch();
uint8_t  mailFailCount();