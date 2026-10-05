#pragma once

#include <Arduino.h>

// Core — send any email. Returns true on successful SMTP send.
bool sendMail(const String& subject, const String& body);

// Convenience wrapper — build formatted content, call sendMail()
bool sendLowBatteryAlert(float voltage, float threshold);

// Rate-limiting / diagnostics
uint32_t mailLastSentEpoch();
uint8_t  mailFailCount();