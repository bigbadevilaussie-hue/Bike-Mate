#pragma once

#include <Arduino.h>

// Bike-Mate GPS module - NEO-6M NMEA parser
// Reads UART on GPS_RX (default GPIO 20), parses NMEA sentences.
// Provides position, altitude, speed, satellite count, fix status.
//
// V4.45

void gpsModuleInit();
void gpsModuleTick();       // call from main loop

bool     gpsHasFix();
int32_t  gpsLat_x1e7();
int32_t  gpsLon_x1e7();
uint8_t  gpsSats();
float    gpsSpeed_kmh();
float    gpsAltitude_m();
uint32_t gpsEpochUTC();     // 0 if no fix/time yet
float    gpsCourseDeg();    // 0 if no fix
const char* gpsTimeUTC();   // "hhmmss" or "--" if none
