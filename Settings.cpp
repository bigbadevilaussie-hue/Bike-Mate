#include "Settings.h"
#include "Config.h"

extern void tprint(const char* fmt, ...);

#define NVS_NAMESPACE "bikeset"

BikeMateConfig config;

static void set_defaults() {
  config.runningEnter_mv = 13800;
  config.runningExit_mv  = 13000;
  config.underRun_mv     = 13800;
}

static uint16_t getU16(Preferences& p, const char* key, uint16_t def) {
  uint16_t v = p.getUShort(key, 0);
  return (v > 0) ? v : def;
}

void settingsLoad() {
  set_defaults();
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  config.runningEnter_mv = getU16(p, "v.run.on",  config.runningEnter_mv);
  config.runningExit_mv  = getU16(p, "v.run.off", config.runningExit_mv);
  config.underRun_mv     = getU16(p, "v.under",   config.underRun_mv);
  p.end();
  tprint("[SETTINGS] loaded: enter=%umV exit=%umV under=%umV",
         config.runningEnter_mv, config.runningExit_mv, config.underRun_mv);
}

void settingsSave() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putUShort("v.run.on",  config.runningEnter_mv);
  p.putUShort("v.run.off", config.runningExit_mv);
  p.putUShort("v.under",   config.underRun_mv);
  p.end();
  tprint("[SETTINGS] saved: enter=%umV exit=%umV under=%umV",
         config.runningEnter_mv, config.runningExit_mv, config.underRun_mv);
}

void settingsReset() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.clear();
  p.end();
  set_defaults();
  tprint("[SETTINGS] reset to defaults");
}

size_t settingsToJson(char* buf, size_t n) {
  return snprintf(buf, n,
    "{\"voltage\":{"
    "\"running_enter\":%.2f,"
    "\"running_exit\":%.2f,"
    "\"under_run\":%.2f"
    "}}",
    config.runningEnter_mv / 1000.0f,
    config.runningExit_mv  / 1000.0f,
    config.underRun_mv     / 1000.0f);
}

static bool extractFloat(const char* json, const char* key, float* out) {
  const char* q = strstr(json, key);
  if (!q) return false;
  q = strchr(q, ':');
  if (!q) return false;
  q++;
  while (*q == ' ' || *q == '\t') q++;
  *out = atof(q);
  return true;
}

bool settingsApplyJson(const char* json) {
  float v;
  uint16_t newEnter = config.runningEnter_mv;
  uint16_t newExit  = config.runningExit_mv;
  uint16_t newUnder = config.underRun_mv;

  if (extractFloat(json, "\"running_enter\"", &v)) {
    if (v < 11.0f || v > 15.0f) { tprint("[SETTINGS] range: enter"); return false; }
    newEnter = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"running_exit\"", &v)) {
    if (v < 10.0f || v > 14.0f) { tprint("[SETTINGS] range: exit"); return false; }
    newExit = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"under_run\"", &v)) {
    if (v < 11.0f || v > 15.0f) { tprint("[SETTINGS] range: under"); return false; }
    newUnder = (uint16_t)(v * 1000.0f + 0.5f);
  }

  if (newExit >= newEnter) {
    tprint("[SETTINGS] reject: exit >= enter");
    return false;
  }

  config.runningEnter_mv = newEnter;
  config.runningExit_mv  = newExit;
  config.underRun_mv     = newUnder;
  settingsSave();
  return true;
}
