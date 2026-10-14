#include "Settings.h"
#include "Config.h"

extern void tprint(const char* fmt, ...);
extern void tprint_verbose(const char* fmt, ...);

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

  // V5.31 SLEEP SCHEDULE
  config.benchMode      = true;
  config.offPeakStart   = 22;
  config.offPeakEnd     = 5;
  config.offPeakWakeMin = 5;   // V5.47: uniform 5 min
  config.peakStart      = 10;
  config.peakEnd        = 13;
  config.peakWakeMin    = 5;   // V5.47: uniform 5 min
  config.dayWakeMin     = 5;

  // V5.31 UPLOAD SCHEDULE
  config.uploadHour = 4;
}

static uint16_t getU16(Preferences& p, const char* key, uint16_t def) {
  uint16_t v = p.getUShort(key, 0);
  return (v > 0) ? v : def;
}

static uint8_t getU8(Preferences& p, const char* key, uint8_t def) {
  // NVS stores an unsigned byte; 0xFF is our "unset" marker (0 is a
  // valid hour, so we cannot use 0 as "unset").
  uint8_t v = p.getUChar(key, 0xFF);
  return (v == 0xFF) ? def : v;
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

  // V5.31 SLEEP + UPLOAD
  config.benchMode      = p.getBool("bench",         config.benchMode);
  config.offPeakStart   = getU8(p, "op.start",  config.offPeakStart);
  config.offPeakEnd     = getU8(p, "op.end",    config.offPeakEnd);
  config.offPeakWakeMin = getU8(p, "op.wake",   config.offPeakWakeMin);
  config.peakStart      = getU8(p, "pk.start",  config.peakStart);
  config.peakEnd        = getU8(p, "pk.end",    config.peakEnd);
  config.peakWakeMin    = getU8(p, "pk.wake",   config.peakWakeMin);
  config.dayWakeMin     = getU8(p, "day.wake",  config.dayWakeMin);
  config.uploadHour     = getU8(p, "up.hour",   config.uploadHour);

  // V5.47: force uniform 5 min. Overrides any stale NVS value.
  config.offPeakWakeMin = 5;
  config.peakWakeMin    = 5;
  config.dayWakeMin     = 5;

  p.end();
  tprint_verbose("[SETTINGS] run: on=%u off=%u un=%u ov=%u | mon: nrm=%u warn=%u pan=%u",
         config.runningEnter_mv, config.runningExit_mv,
         config.runUnder_mv, config.runOver_mv,
         config.monitorNormal_mv, config.monitorWarning_mv, config.monitorPanic_mv);
  tprint_verbose("[SETTINGS] sleep: bench=%d op=%u-%u/%um pk=%u-%u/%um day=%um up=%u",
         config.benchMode ? 1 : 0,
         config.offPeakStart, config.offPeakEnd, config.offPeakWakeMin,
         config.peakStart, config.peakEnd, config.peakWakeMin,
         config.dayWakeMin, config.uploadHour);
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

  // V5.31 SLEEP + UPLOAD
  p.putBool("bench",    config.benchMode);
  p.putUChar("op.start", config.offPeakStart);
  p.putUChar("op.end",   config.offPeakEnd);
  p.putUChar("op.wake",  config.offPeakWakeMin);
  p.putUChar("pk.start", config.peakStart);
  p.putUChar("pk.end",   config.peakEnd);
  p.putUChar("pk.wake",  config.peakWakeMin);
  p.putUChar("day.wake", config.dayWakeMin);
  p.putUChar("up.hour",  config.uploadHour);

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
    "},"
    "\"s\":{"
      "\"b\":%d,\"ops\":%d,\"ope\":%d,\"opw\":%d,"
      "\"pks\":%d,\"pke\":%d,\"pkw\":%d,\"dw\":%d,"
      "\"uh\":%d"
    "}}",
    config.runningEnter_mv   / 1000.0f,
    config.runningExit_mv    / 1000.0f,
    config.runUnder_mv       / 1000.0f,
    config.runOver_mv        / 1000.0f,
    config.monitorNormal_mv  / 1000.0f,
    config.monitorWarning_mv / 1000.0f,
    config.monitorPanic_mv   / 1000.0f,
    config.benchMode ? 1 : 0,
    config.offPeakStart,
    config.offPeakEnd,
    config.offPeakWakeMin,
    config.peakStart,
    config.peakEnd,
    config.peakWakeMin,
    config.dayWakeMin,
    config.uploadHour);
}

static bool extractInt(const char* json, const char* key, int* out) {
  const char* q = strstr(json, key);
  if (!q) return false;
  q = strchr(q, ':');
  if (!q) return false;
  q++;
  while (*q == ' ') q++;
  *out = atoi(q);
  return true;
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

  // V5.31 extract sleep + upload
  int iv;
  bool bv = config.benchMode;
  uint8_t opS = config.offPeakStart;
  uint8_t opE = config.offPeakEnd;
  uint8_t opW = config.offPeakWakeMin;
  uint8_t pkS = config.peakStart;
  uint8_t pkE = config.peakEnd;
  uint8_t pkW = config.peakWakeMin;
  uint8_t dW  = config.dayWakeMin;
  uint8_t uH  = config.uploadHour;
  if (extractInt(json, "\"b\"", &iv)) {
    if (iv < 0 || iv > 1) { tprint("[SETTINGS] range: bench"); return false; }
    bv = (iv != 0);
  }
  if (extractInt(json, "\"ops\"", &iv)) {
    if (iv < 0 || iv > 23) { tprint("[SETTINGS] range: ops"); return false; }
    opS = (uint8_t)iv;
  }
  if (extractInt(json, "\"ope\"", &iv)) {
    if (iv < 0 || iv > 23) { tprint("[SETTINGS] range: ope"); return false; }
    opE = (uint8_t)iv;
  }
  if (extractInt(json, "\"opw\"", &iv)) {
    if (iv < 1 || iv > 30) { tprint("[SETTINGS] range: opw"); return false; }
    opW = (uint8_t)iv;
  }
  if (extractInt(json, "\"pks\"", &iv)) {
    if (iv < 0 || iv > 23) { tprint("[SETTINGS] range: pks"); return false; }
    pkS = (uint8_t)iv;
  }
  if (extractInt(json, "\"pke\"", &iv)) {
    if (iv < 0 || iv > 23) { tprint("[SETTINGS] range: pke"); return false; }
    pkE = (uint8_t)iv;
  }
  if (extractInt(json, "\"pkw\"", &iv)) {
    if (iv < 1 || iv > 30) { tprint("[SETTINGS] range: pkw"); return false; }
    pkW = (uint8_t)iv;
  }
  if (extractInt(json, "\"dw\"", &iv)) {
    if (iv < 1 || iv > 30) { tprint("[SETTINGS] range: dw"); return false; }
    dW = (uint8_t)iv;
  }
  if (extractInt(json, "\"uh\"", &iv)) {
    if (iv < 0 || iv > 23) { tprint("[SETTINGS] range: uh"); return false; }
    uH = (uint8_t)iv;
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

  config.benchMode      = bv;
  config.offPeakStart   = opS;
  config.offPeakEnd     = opE;
  config.offPeakWakeMin = opW;
  config.peakStart      = pkS;
  config.peakEnd        = pkE;
  config.peakWakeMin    = pkW;
  config.dayWakeMin     = dW;
  config.uploadHour     = uH;

  settingsSave();
  return true;
}
