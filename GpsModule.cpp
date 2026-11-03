#include "GpsModule.h"
#include "Config.h"

#include <time.h>
#include <TinyGPSPlus.h>

static TinyGPSPlus gps;

extern void tprint(const char* fmt, ...);

// GPS UART pins - on the C3 SuperMini, UART0 on GPIO 20/21 is free
// because the USB console uses USB-Serial-JTAG.
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 2
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 21
#endif
#define GPS_BAUD 9600

void gpsModuleTick() {
  while (Serial1.available()) {
    gps.encode(Serial1.read());
  }
}

void gpsModuleInit() {
  Serial1.setRxBufferSize(256);
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(300);
  int avail = Serial1.available();
  tprint("[GPS] UART init RX=%d TX=%d baud=%d avail=%d",
         GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD, avail);
}


bool gpsHasFix() { return gps.location.isValid(); }
int32_t gpsLat_x1e7() { return gps.location.isValid() ? (int32_t)(gps.location.lat() * 1e7) : 0; }
int32_t gpsLon_x1e7() { return gps.location.isValid() ? (int32_t)(gps.location.lng() * 1e7) : 0; }
uint8_t gpsSats() { return gps.satellites.isValid() ? gps.satellites.value() : 0; }
float gpsSpeed_kmh() { return gps.speed.isValid() ? gps.speed.kmph() : 0.0f; }
float gpsAltitude_m() { return gps.altitude.isValid() ? gps.altitude.meters() : 0.0f; }
float gpsCourseDeg() { return gps.course.isValid() ? gps.course.deg() : 0.0f; }
const char* gpsTimeUTC() {
  static char buf[8];
  if (gps.time.isValid()) {
    snprintf(buf, sizeof(buf), "%02d%02d%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
    return buf;
  }
  return "--";
}
uint32_t gpsEpochUTC() { return 0; }
uint32_t gpsCharsProcessed() { return gps.charsProcessed(); }
uint32_t gpsFailedChecksum() { return gps.failedChecksum(); }



















