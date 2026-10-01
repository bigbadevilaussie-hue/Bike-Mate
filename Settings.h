#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Bike-Mate Settings — runtime-tunable, persisted in NVS "bikeset"
// Rules: defaults in set_defaults() only; NVS keys <15 chars; zero = unset.
// NVS keys: v.run.on(8) v.run.off(9) v.under(7)

struct BikeMateConfig {
  uint16_t runningEnter_mv;
  uint16_t runningExit_mv;
  uint16_t underRun_mv;
};

extern BikeMateConfig config;
void   settingsLoad();
void   settingsSave();
void   settingsReset();
bool   settingsApplyJson(const char* json);
size_t settingsToJson(char* buf, size_t n);
