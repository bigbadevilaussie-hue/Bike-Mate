// WakeLogger.cpp — BIKE-MATE V3.44
// Writes one sample per wake to /wakes_YYYY-MM-DD.csv.
// V3.44: [WAKE] sample print behind DEBUG_VERBOSE.
// V3.32: force-rotate skipped if a sealed file already exists (prevents data loss).
// V3.17: force-rotate for bench mode.
// V3.02: date filenames, pause/resume, upload event logging.

#include "WakeLogger.h"
#include "Config.h"
#include "RideLogger.h"
#include "Sensors.h"
#include <LittleFS.h>
#include <time.h>

extern void tprint(const char* fmt, ...);
extern uint32_t currentEpoch();
extern float latestBatteryVoltage;
extern float latestTemperatureC;
extern bool inPanic;

static int32_t _lat_x1e7 = 0;
static int32_t _lon_x1e7 = 0;
static uint8_t _sats = 0;

static char _currentPath[48] = "";
static bool _paused = false;

// ---- helpers ----

static void buildFilename(uint32_t epoch, char* buf, size_t n) {
  time_t t = epoch;
  struct tm* ti = localtime(&t);
  snprintf(buf, n, "/wakes_%04d-%02d-%02d.csv",
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
}

static void writeHeader(File& f, uint32_t epoch) {
  f.print("# fw=");
  f.println(BIKE_MATE_VERSION);

  time_t t = epoch;
  struct tm* ti = localtime(&t);
  char buf[80];
  snprintf(buf, sizeof(buf),
           "# opened=%lu (%04d-%02d-%02d %02d:%02d)",
           (unsigned long)epoch,
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
           ti->tm_hour, ti->tm_min);
  f.println(buf);

  f.println("epoch,lat,lon,volt,temp,state,flags,sats");
}

static bool openCurrentFile(uint32_t epoch) {
  buildFilename(epoch, _currentPath, sizeof(_currentPath));

  if (LittleFS.exists(_currentPath)) {
    return true;
  }

  File f = LittleFS.open(_currentPath, "w");
  if (!f) {
    tprint("[WAKE] can't create %s", _currentPath);
    _currentPath[0] = 0;
    return false;
  }
  writeHeader(f, epoch);
  f.close();
  tprint("[WAKE] created %s", _currentPath);
  return true;
}

// ---- init ----
bool wakeLoggerInit() {
  if (!LittleFS.begin(true)) {
    tprint("[WAKE] LittleFS mount failed");
    return false;
  }

  uint32_t now = currentEpoch();
  if (now == 0) {
    tprint("[WAKE] no epoch, deferring file open");
    return false;
  }

  if (!openCurrentFile(now)) {
    return false;
  }

  tprint("[WAKE] ready, current=%s size=%u",
         _currentPath, (unsigned)wakeLoggerFileSize());
  return true;
}

// ---- flags ----
static uint8_t _buildFlags() {
  uint8_t f = 0;
  if (inPanic) f |= FLAG_PANIC;
  if (_sats > 0) f |= FLAG_GPS_FIX;
  return f;
}

// ---- write one row ----
static void _writeSample(uint32_t epoch) {
  if (_paused) return;
  if (_currentPath[0] == 0) {
    if (!openCurrentFile(epoch)) return;
  }

  File f = LittleFS.open(_currentPath, "a");
  if (!f) {
    tprint("[WAKE] open failed %s", _currentPath);
    return;
  }

  char row[96];
  int n = snprintf(row, sizeof(row),
                   "%lu,%ld,%ld,%.2f,%d,%d,0x%02X,%d\n",
                   (unsigned long)epoch,
                   (long)_lat_x1e7, (long)_lon_x1e7,
                   latestBatteryVoltage,
                   (int)latestTemperatureC,
                   0,
                   (int)_buildFlags(),
                   (int)_sats);
  if (n > 0) f.write((uint8_t*)row, n);
  f.close();
}

// ---- pause / resume ----
void wakeLoggerPause() {
  _paused = true;
  tprint("[WAKE] paused");
}

void wakeLoggerResume() {
  _paused = false;
  tprint("[WAKE] resumed");
}

bool wakeLoggerIsPaused() {
  return _paused;
}

// ---- tick ----
void wakeLoggerTick() {
  if (_paused) return;
  if (isLogging) return;

  uint32_t now = currentEpoch();
  if (now == 0) return;

  _writeSample(now);
#if DEBUG_VERBOSE
  tprint("[WAKE] sample @ %lu V=%.2f T=%d",
         (unsigned long)now, latestBatteryVoltage,
         (int)latestTemperatureC);
#endif
}

// ---- force write ----
void wakeLoggerForceWrite() {
  if (_paused) return;

  uint32_t now = currentEpoch();
  if (now == 0) return;

  _writeSample(now);
  tprint("[WAKE] force @ %lu", (unsigned long)now);
}

// ---- rotate (called at 04:00 trigger) ----
bool wakeLoggerRotate(uint32_t triggerEpoch) {
  tprint("[WAKE] rotating at %lu", (unsigned long)triggerEpoch);

  char newPath[48];
  buildFilename(triggerEpoch, newPath, sizeof(newPath));

  if (strcmp(newPath, _currentPath) == 0) {
    tprint("[WAKE] same day, no rotation");
    return false;
  }

  strncpy(_currentPath, newPath, sizeof(_currentPath) - 1);
  _currentPath[sizeof(_currentPath) - 1] = 0;

  File f = LittleFS.open(_currentPath, "w");
  if (!f) {
    tprint("[WAKE] can't create %s", _currentPath);
    _currentPath[0] = 0;
    return false;
  }
  writeHeader(f, triggerEpoch);
  f.close();
  tprint("[WAKE] rotated to %s", _currentPath);
  return true;
}

// ---- V3.17: force rotate for bench mode ----
bool wakeLoggerForceRotate(uint32_t triggerEpoch) {
  if (_currentPath[0] == 0) {
    // No current file — try to open one
    return openCurrentFile(triggerEpoch);
  }

  // V3.32: check for pending sealed file FIRST
  char sealed[64];
  snprintf(sealed, sizeof(sealed), "%s.sealed", _currentPath);

  if (LittleFS.exists(sealed)) {
    // Previous seal never uploaded. Do NOT rotate — that would destroy it.
    tprint("[WAKE] pending seal exists, skipping rotate: %s", sealed);
    return false;
  }

  // Rename current to .sealed
  if (!LittleFS.rename(_currentPath, sealed)) {
    tprint("[WAKE] rename failed %s", _currentPath);
    return false;
  }
  tprint("[WAKE] sealed as %s", sealed);

  // Clear current, open fresh
  _currentPath[0] = 0;
  if (!openCurrentFile(triggerEpoch)) {
    tprint("[WAKE] new file create failed");
    return false;
  }
  tprint("[WAKE] new file %s", _currentPath);
  return true;
}

// ---- upload logging ----
void wakeLoggerLogUploadStart() {
  if (_paused) return;
  uint32_t now = currentEpoch();
  if (now == 0 || _currentPath[0] == 0) return;

  File f = LittleFS.open(_currentPath, "a");
  if (!f) return;
  char buf[64];
  snprintf(buf, sizeof(buf), "UPLOAD,start,%lu\n", (unsigned long)now);
  f.write((uint8_t*)buf, strlen(buf));
  f.close();
}

void wakeLoggerLogUpload(const char* filename, bool ok, const char* detail) {
  uint32_t now = currentEpoch();
  if (now == 0 || _currentPath[0] == 0) return;

  File f = LittleFS.open(_currentPath, "a");
  if (!f) return;
  char buf[128];
  if (detail && detail[0]) {
    snprintf(buf, sizeof(buf), "UPLOAD,%s,%s,%s\n",
             filename, ok ? "OK" : "FAIL", detail);
  } else {
    snprintf(buf, sizeof(buf), "UPLOAD,%s,%s\n",
             filename, ok ? "OK" : "FAIL");
  }
  f.write((uint8_t*)buf, strlen(buf));
  f.close();
}

void wakeLoggerLogUploadDone(int okCount, int failCount) {
  uint32_t now = currentEpoch();
  if (now == 0 || _currentPath[0] == 0) return;

  File f = LittleFS.open(_currentPath, "a");
  if (!f) return;
  char buf[80];
  snprintf(buf, sizeof(buf), "UPLOAD,done,%d ok %d fail\n", okCount, failCount);
  f.write((uint8_t*)buf, strlen(buf));
  f.close();
}

// ---- file ops ----
size_t wakeLoggerFileSize() {
  if (_currentPath[0] == 0) return 0;
  File f = LittleFS.open(_currentPath, "r");
  if (!f) return 0;
  size_t s = f.size();
  f.close();
  return s;
}

bool wakeLoggerTruncate() {
  if (_currentPath[0] == 0) return false;
  uint32_t now = currentEpoch();
  if (now == 0) return false;

  File f = LittleFS.open(_currentPath, "w");
  if (!f) return false;
  writeHeader(f, now);
  f.close();
  return true;
}

// ---- current filename ----
const char* wakeLoggerCurrentFile() {
  return _currentPath;
}

// ---- GPS hook ----
void wakeLoggerSetLocation(int32_t lat_x1e7, int32_t lon_x1e7, uint8_t sats) {
  _lat_x1e7 = lat_x1e7;
  _lon_x1e7 = lon_x1e7;
  _sats = sats;
}