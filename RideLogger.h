#pragma once

#include <Arduino.h>
#include "Config.h"

// RideLogger — owns the ride lifecycle state machine.
// On ride start: opens a new file via RideStorage.
// During ride: writes rows every LOG_INTERVAL_SEC (5 s).
// On ride close: writes summary to NVS, closes file.

// RTC-persistent ride state — survives deep sleep
extern bool isLogging;
extern uint32_t rideStartEpoch;
extern uint32_t lastLogEpoch;
extern uint16_t rideMinV;
extern uint16_t rideMaxV;
extern uint32_t rideSumV;
extern int8_t   rideMinT;
extern int8_t   rideMaxT;
extern uint16_t rideUnderSecs;
extern uint16_t rideOverSecs;
extern uint16_t currentRowCount;

// Non-RTC ride state
extern float ridePreVoltage;
extern int32_t rideStartLat_x1e7;
extern int32_t rideStartLon_x1e7;

// Lifecycle
void loadRideState();
void startRideLog();
void writeRideRow();
void closeRideLog();

// Set ride start location (called from bike_mate.ino during engine start)
void setRideStartLocation(int32_t lat_x1e7, int32_t lon_x1e7);