// RideLogger.cpp — BIKE-MATE V2.70
// Ride lifecycle. Rows go to LittleFS via RideStorage.
// Summary written to NVS "rides" namespace on close.

#include "RideLogger.h"
#include "RideStorage.h"
#include "Sensors.h"

#include <Preferences.h>
#include <Arduino.h>

extern Preferences prefs;
extern void tprint(const char* fmt, ...);
extern uint32_t currentEpoch();
extern float latestBatteryVoltage;
extern float latestTemperatureC;

RTC_DATA_ATTR bool isLogging = false;
RTC_DATA_ATTR uint32_t rideStartEpoch = 0;
RTC_DATA_ATTR uint32_t lastLogEpoch = 0;
RTC_DATA_ATTR uint16_t rideMinV = 0xFFFF;
RTC_DATA_ATTR uint16_t rideMaxV = 0;
RTC_DATA_ATTR uint32_t rideSumV = 0;
RTC_DATA_ATTR int8_t   rideMinT = 127;
RTC_DATA_ATTR int8_t   rideMaxT = -128;
RTC_DATA_ATTR uint16_t rideUnderSecs = 0;
RTC_DATA_ATTR uint16_t rideOverSecs = 0;
RTC_DATA_ATTR uint16_t currentRowCount = 0;

float ridePreVoltage = 0.0;
int32_t rideStartLat_x1e7 = 0;
int32_t rideStartLon_x1e7 = 0;

void loadRideState() {
  tprint("[RIDE] state: isLogging=%d startEpoch=%lu rows=%d",
         isLogging ? 1 : 0,
         (unsigned long)rideStartEpoch,
         (int)currentRowCount);
}

void startRideLog() {
  if (isLogging) return;

  uint32_t now = currentEpoch();
  if (now == 0) {
    tprint("[RIDE] no clock, deferring start");
    return;
  }

  tprint("[RIDE] START epoch=%lu preV=%.2f",
         (unsigned long)now, ridePreVoltage);

  if (!rideStorageCreate(now)) {
    tprint("[RIDE] file create failed");
    return;
  }

  isLogging = true;
  currentRowCount = 0;
  rideStartEpoch = now;
  lastLogEpoch = 0;
  rideMinV = 0xFFFF; rideMaxV = 0; rideSumV = 0;
  rideMinT = 127; rideMaxT = -128;
  rideUnderSecs = 0; rideOverSecs = 0;
}

void writeRideRow() {
  if (!isLogging) return;

  uint32_t epoch = currentEpoch();
  if (epoch == 0) return;
  if (lastLogEpoch != 0 && epoch - lastLogEpoch < LOG_INTERVAL_SEC) return;

  uint16_t v100 = (uint16_t)(latestBatteryVoltage * 100);
  int8_t   t    = (int8_t)latestTemperatureC;

  RideRow row;
  row.epoch     = epoch;
  row.lat_x1e7  = 0;
  row.lon_x1e7  = 0;
  row.volt_x100 = v100;
  row.temp      = t;
  row.state     = 1;

  if (!rideStorageAppendRow(row)) {
    tprint("[RIDE] append failed");
    return;
  }

  if (v100 < rideMinV) rideMinV = v100;
  if (v100 > rideMaxV) rideMaxV = v100;
  rideSumV += v100;
  if (t < rideMinT) rideMinT = t;
  if (t > rideMaxT) rideMaxT = t;
  if (latestBatteryVoltage < TH_UNDER_RUN) rideUnderSecs += LOG_INTERVAL_SEC;
  if (latestBatteryVoltage > TH_OVER_VOLT) rideOverSecs += LOG_INTERVAL_SEC;

  currentRowCount++;
  lastLogEpoch = epoch;

  if (currentRowCount % 12 == 0) {
    tprint("[RIDE] row %d @ %lu %.2fV %.1fC",
           (int)currentRowCount, (unsigned long)epoch,
           latestBatteryVoltage, latestTemperatureC);
  }
}

void closeRideLog() {
  if (!isLogging) return;

  tprint("[RIDE] END rows=%d", (int)currentRowCount);
  isLogging = false;

  if (currentRowCount == 0) {
    rideStorageClose();
    return;
  }

  uint32_t nowEpoch = currentEpoch();
  if (nowEpoch == 0 || nowEpoch < rideStartEpoch) {
    tprint("[RIDE] clock invalid, closing without summary");
    rideStorageClose();
    currentRowCount = 0;
    return;
  }

  RideSummary s;
  memset(&s, 0, sizeof(s));
  s.startEpoch     = rideStartEpoch;
  s.endEpoch       = nowEpoch;
  s.durationSecs   = nowEpoch - rideStartEpoch;
  s.minVolt_x100   = rideMinV;
  s.maxVolt_x100   = rideMaxV;
  s.avgVolt_x100   = (uint16_t)(rideSumV / currentRowCount);
  s.minTemp        = rideMinT;
  s.maxTemp        = rideMaxT;
  s.underVoltSecs  = rideUnderSecs;
  s.overVoltSecs   = rideOverSecs;
  s.rowCount       = currentRowCount;
  s.flags          = 0;
  s.preRideVolt_x100 = (uint16_t)(ridePreVoltage * 100);
  s.startLat_x1e7  = rideStartLat_x1e7;
  s.startLon_x1e7  = rideStartLon_x1e7;
  s.endLat_x1e7    = 0;
  s.endLon_x1e7    = 0;
  s.rowCount_hi    = 0;
  s.pad            = 0;

  if (rideUnderSecs > 0)           s.flags |= FLAG_UNDER_VOLT;
  if (rideOverSecs > 0)            s.flags |= FLAG_OVER_VOLT;
  if (rideMinT < TH_FREEZING)      s.flags |= FLAG_FREEZING;
  if (rideMaxT > TH_HOT_AMBIENT)   s.flags |= FLAG_HOT_AMBIENT;
  if (rideMaxT > TH_ENCLOSURE_HOT) s.flags |= FLAG_ENCLOSURE_HOT;

  if (s.durationSecs == 0) {
    tprint("[RIDE] zero duration, closing file without summary");
    rideStorageClose();
    currentRowCount = 0;
    return;
  }

  prefs.begin("rides", false);
  char key[16];
  snprintf(key, sizeof(key), "s%lu", (unsigned long)rideStartEpoch);
  prefs.putBytes(key, &s, sizeof(s));
  prefs.putUInt("newest_epoch", rideStartEpoch);
  prefs.end();

  rideStorageClose();

  tprint("[RIDE] saved summary key=%s dur=%lu rows=%d",
         key, (unsigned long)s.durationSecs, (int)s.rowCount);

  currentRowCount = 0;
}

void setRideStartLocation(int32_t lat_x1e7, int32_t lon_x1e7) {
  rideStartLat_x1e7 = lat_x1e7;
  rideStartLon_x1e7 = lon_x1e7;
}