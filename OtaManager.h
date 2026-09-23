#pragma once

#include <Arduino.h>

// Called from bike_mate.ino when otaRequest becomes true.
// Connects WiFi, fetches firmware from the URL received over BLE,
// flashes to the inactive OTA partition, reboots on success.
// Never returns on success (device reboots).
// Returns false on failure (device stays running, clears flag).
bool otaPerformUpdate(const char* url, uint32_t expectedSize, const char* expectedMd5);

// Set by BLE callback. Read by loop() and doStateWork().
extern volatile bool otaRequest;
extern char otaUrl[];
extern uint32_t otaSize;
extern char otaMd5[];

// Version string sent by the GUI in the OTA payload.
extern char otaVersion[16];

// Timestamp when OTA mode started. Used for the awake timeout.
extern unsigned long otaStartMillis;

// Progress tracking for the OLED screen.
extern volatile uint32_t otaProgressBytes;
extern volatile uint32_t otaProgressTotal;
extern volatile uint8_t  otaStage;
extern volatile uint8_t  otaRebootCountdown;

// Stage defines
#define OTA_STAGE_IDLE      0
#define OTA_STAGE_DOWNLOAD  1
#define OTA_STAGE_VERIFY    2
#define OTA_STAGE_FLASH     3
#define OTA_STAGE_REBOOT    4

// Signal to the rest of the firmware that we're in OTA mode (blocks sleep).
bool otaInProgress();