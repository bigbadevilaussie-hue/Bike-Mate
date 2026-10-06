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
};

extern BikeMateConfig config;
void   settingsLoad();
void   settingsSave();
void   settingsReset();
bool   settingsApplyJson(const char* json);
size_t settingsToJson(char* buf, size_t n);
