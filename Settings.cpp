#include "Settings.h"
#include "Config.h"

extern void tprint(const char* fmt, ...);

#define NVS_NAMESPACE "bikeset"

BikeMateConfig config;

static void set_defaults() {
  // RUN MODE
  config.runningEnter_mv  = 13800;
  config.runningExit_mv   = 13000;
  config.runUnder_mv      = 13000;
  config.runOver_mv       = 14800;

  // MONITOR MODE
  config.monitorNormal_mv  = 12500;
  config.monitorWarning_mv = 12400;
  config.monitorPanic_mv   = 12200;
}

static uint16_t getU16(Preferences& p, const char* key, uint16_t def) {
  uint16_t v = p.getUShort(key, 0);
  return (v > 0) ? v : def;
}

void settingsLoad() {
  set_defaults();
  Preferences p;
  p.begin(NVS_NAMESPACE, true);
  config.runningEnter_mv  = getU16(p, "v.run.on",   config.runningEnter_mv);
  config.runningExit_mv   = getU16(p, "v.run.off",  config.runningExit_mv);
  config.runUnder_mv      = getU16(p, "v.run.un",   config.runUnder_mv);
  config.runOver_mv       = getU16(p, "v.run.ov",   config.runOver_mv);
  config.monitorNormal_mv = getU16(p, "v.mon.nrm",  config.monitorNormal_mv);
  config.monitorWarning_mv= getU16(p, "v.mon.warn", config.monitorWarning_mv);
  config.monitorPanic_mv  = getU16(p, "v.mon.pan",  config.monitorPanic_mv);
  p.end();
  tprint("[SETTINGS] run: on=%u off=%u un=%u ov=%u | mon: nrm=%u warn=%u pan=%u",
         config.runningEnter_mv, config.runningExit_mv,
         config.runUnder_mv, config.runOver_mv,
         config.monitorNormal_mv, config.monitorWarning_mv, config.monitorPanic_mv);
}

void settingsSave() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putUShort("v.run.on",   config.runningEnter_mv);
  p.putUShort("v.run.off",  config.runningExit_mv);
  p.putUShort("v.run.un",   config.runUnder_mv);
  p.putUShort("v.run.ov",   config.runOver_mv);
  p.putUShort("v.mon.nrm",  config.monitorNormal_mv);
  p.putUShort("v.mon.warn", config.monitorWarning_mv);
  p.putUShort("v.mon.pan",  config.monitorPanic_mv);
  p.end();
  tprint("[SETTINGS] saved");
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
  // V5.02: compact keys and 1 decimal. The C3 + Mac BLE link
  // negotiates ~100 byte notify MTU, and bleak delivers each
  // fragment as a separate on_notify. Payload must fit in one
  // notify or the GUI sees a truncated string.
  return snprintf(buf, n,
    "{\"r\":{"
      "\"on\":%.1f,\"off\":%.1f,\"un\":%.1f,\"ov\":%.1f"
    "},"
    "\"m\":{"
      "\"nrm\":%.1f,\"wrn\":%.1f,\"pan\":%.1f"
    "}}",
    config.runningEnter_mv   / 1000.0f,
    config.runningExit_mv    / 1000.0f,
    config.runUnder_mv       / 1000.0f,
    config.runOver_mv        / 1000.0f,
    config.monitorNormal_mv  / 1000.0f,
    config.monitorWarning_mv / 1000.0f,
    config.monitorPanic_mv   / 1000.0f);
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
  uint16_t nEnter  = config.runningEnter_mv;
  uint16_t nExit   = config.runningExit_mv;
  uint16_t nUnder  = config.runUnder_mv;
  uint16_t nOver   = config.runOver_mv;
  uint16_t nNormal = config.monitorNormal_mv;
  uint16_t nWarn   = config.monitorWarning_mv;
  uint16_t nPanic  = config.monitorPanic_mv;

  // V5.02: accept both compact (r.on) and legacy long keys
  if (extractFloat(json, "\"on\"", &v) || extractFloat(json, "\"running_enter\"", &v)) {
    if (v < 12.0f || v > 15.0f) { tprint("[SETTINGS] range: enter"); return false; }
    nEnter = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"off\"", &v) || extractFloat(json, "\"running_exit\"", &v)) {
    if (v < 11.5f || v > 14.5f) { tprint("[SETTINGS] range: exit"); return false; }
    nExit = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"un\"", &v) || extractFloat(json, "\"run_under\"", &v)) {
    if (v < 11.5f || v > 14.8f) { tprint("[SETTINGS] range: run_under"); return false; }
    nUnder = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"ov\"", &v) || extractFloat(json, "\"run_over\"", &v)) {
    if (v < 13.5f || v > 16.0f) { tprint("[SETTINGS] range: run_over"); return false; }
    nOver = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"nrm\"", &v) || extractFloat(json, "\"normal\"", &v)) {
    if (v < 11.8f || v > 13.0f) { tprint("[SETTINGS] range: normal"); return false; }
    nNormal = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"wrn\"", &v) || extractFloat(json, "\"warning\"", &v)) {
    if (v < 11.5f || v > 13.0f) { tprint("[SETTINGS] range: warning"); return false; }
    nWarn = (uint16_t)(v * 1000.0f + 0.5f);
  }
  if (extractFloat(json, "\"pan\"", &v) || extractFloat(json, "\"panic\"", &v)) {
    if (v < 11.0f || v > 12.5f) { tprint("[SETTINGS] range: panic"); return false; }
    nPanic = (uint16_t)(v * 1000.0f + 0.5f);
  }

  // cross-field
  if (nExit >= nEnter) {
    tprint("[SETTINGS] reject: exit >= enter");
    return false;
  }
  if (nUnder >= nOver) {
    tprint("[SETTINGS] reject: run_under >= run_over");
    return false;
  }
  if (!(nPanic < nWarn && nWarn < nNormal)) {
    tprint("[SETTINGS] reject: panic < warning < normal required");
    return false;
  }
  if (nNormal > nExit) {
    tprint("[SETTINGS] reject: monitor normal above run exit");
    return false;
  }

  config.runningEnter_mv  = nEnter;
  config.runningExit_mv   = nExit;
  config.runUnder_mv      = nUnder;
  config.runOver_mv       = nOver;
  config.monitorNormal_mv = nNormal;
  config.monitorWarning_mv= nWarn;
  config.monitorPanic_mv  = nPanic;
  settingsSave();
  return true;
}
