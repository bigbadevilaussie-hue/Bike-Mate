#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Bike-Mate Settings — runtime-tunable, persisted in NVS "bikeset"
// Rules: defaults in set_defaults() only; NVS keys <15 chars; zero = unset.
//
// V5.02 schema: two modes.
//   RUN MODE     — engine running (V >= runningEnter_mv)
//   MONITOR MODE — engine off
//
// NVS keys: v.run.on(8) v.run.off(9) v.run.un(8) v.run.ov(8)
//           v.mon.nrm(9) v.mon.warn(10) v.mon.pan(9)

struct BikeMateConfig {
  // RUN MODE
  uint16_t runningEnter_mv;   // engine start threshold
  uint16_t runningExit_mv;    // engine stop threshold
  uint16_t runUnder_mv;       // running but voltage below this = charging issue
  uint16_t runOver_mv;        // running but voltage above this = regulator issue

  // MONITOR MODE
  uint16_t monitorNormal_mv;  // resting voltage at or above = healthy
  uint16_t monitorWarning_mv; // resting voltage below = email
  uint16_t monitorPanic_mv;   // resting voltage below = PANIC / prime directive

  // V5.31 SLEEP SCHEDULE
  bool     benchMode;         // 1 = 30s wakes, 0 = three-zone schedule
  uint8_t  offPeakStart;      // hour 0-23, off-peak window start
  uint8_t  offPeakEnd;        // hour 0-23, off-peak window end
  uint8_t  offPeakWakeMin;    // 1-30, wake cadence in off-peak
  uint8_t  peakStart;         // hour 0-23, day-peak window start
  uint8_t  peakEnd;           // hour 0-23, day-peak window end
  uint8_t  peakWakeMin;       // 1-30, wake cadence in day-peak
  uint8_t  dayWakeMin;        // 1-30, wake cadence in day (all other hours)

  // V5.31 UPLOAD SCHEDULE
  uint8_t  uploadHour;        // hour 0-23, daily upload trigger
};

extern BikeMateConfig config;
void   settingsLoad();
void   settingsSave();
void   settingsReset();
bool   settingsApplyJson(const char* json);
size_t settingsToJson(char* buf, size_t n);
