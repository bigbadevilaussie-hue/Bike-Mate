// WakeLogger.cpp — BIKE-MATE V3.44
// Writes one sample per wake to /wakes_YYYY-MM-DD.csv.
// V3.44: [WAKE] sample print behind DEBUG_VERBOSE.
// V3.32: force-rotate skipped if a sealed file already exists (prevents data loss).
// V3.17: force-rotate for bench mode.
// V3.02: date filenames, pause/resume, upload event logging.

#define DEST_FS_USES_LITTLEFS
#include <ESP32-targz.h>
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
  setenv("TZ", "AEST-10", 1);
  tzset();
  time_t t = epoch;
  struct tm* ti = localtime(&t);
  snprintf(buf, n, "/wakes_%04d-%02d-%02d.csv",
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
}

// V5.17: sealed files get HHMMSS so each seal is a unique upload
// path. GitHub Contents API rejects a PUT without "sha" when the
// target path already exists; unique names avoid the probe-GET.
static void buildSealedName(uint32_t epoch, char* buf, size_t n) {
  setenv("TZ", "AEST-10", 1);
  tzset();
  time_t t = epoch;
  struct tm* ti = localtime(&t);
  snprintf(buf, n, "/wakes_%04d-%02d-%02d_%02d%02d%02d.csv",
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
           ti->tm_hour, ti->tm_min, ti->tm_sec);
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
  extern bool gpsHasFix();
  uint8_t f = 0;
  if (inPanic) f |= FLAG_PANIC;
  if (gpsHasFix()) f |= FLAG_GPS_FIX;

  // V4.57: storage level check - set bit when LittleFS > 75%
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  if (total > 0 && (used * 100 / total) >= 75) {
    f |= FLAG_STORAGE_LOW;
  }
  return f;
}

// ---- write one row ----
static void _writeSample(uint32_t epoch, int stateOverride = -1) {
  if (_paused) return;
  if (_currentPath[0] == 0) {
    if (!openCurrentFile(epoch)) return;
  }

  File f = LittleFS.open(_currentPath, "a");
  if (!f) {
    tprint("[WAKE] open failed %s", _currentPath);
    return;
  }

  extern bool inPanic;
  extern bool engineWasRunning;
  extern bool accState;
  extern bool isCountingDown;
  extern bool isArmingCountdown;

  int state = 0;
  if (stateOverride >= 0) {
    state = stateOverride;
  } else if (inPanic) state = 2;
  else if (engineWasRunning || accState) state = 1;
  else if (isCountingDown) state = 3;
  else if (isArmingCountdown) state = 4;

  char row[96];
  int n = snprintf(row, sizeof(row),
                   "%lu,%ld,%ld,%.2f,%d,%d,0x%02X,%d\n",
                   (unsigned long)epoch,
                   (long)_lat_x1e7, (long)_lon_x1e7,
                   latestBatteryVoltage,
                   (int)latestTemperatureC,
                   state,
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

  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  if (total > 0) {
    tprint("[STORAGE] %u%% used (%u/%u bytes)",
           (unsigned)((used * 100) / total),
           (unsigned)used, (unsigned)total);
  }

  _writeSample(now, -1);
#if DEBUG_VERBOSE
  tprint("[WAKE] sample @ %lu V=%.2f T=%d",
         (unsigned long)now, latestBatteryVoltage,
         (int)latestTemperatureC);
#endif
}

// ---- force write ----
void wakeLoggerForceWrite(int state) {
  if (_paused) return;

  uint32_t now = currentEpoch();
  if (now == 0) return;

  _writeSample(now, state);
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

  // V5.17: unique sealed name includes HHMMSS so each seal is a
  // distinct upload path on GitHub (avoids 422 "sha" required).
  char sealedBase[64];
  buildSealedName(triggerEpoch, sealedBase, sizeof(sealedBase));
  char sealed[80];
  snprintf(sealed, sizeof(sealed), "%s.sealed", sealedBase);

  // V5.17: scan for any pending .sealed file. With unique HHMMSS
  // names, checking a single constructed path won't catch a seal
  // from a prior cycle that failed to upload. If any wake file
  // ends in .sealed, skip this rotation rather than risk losing it.
  {
    File root = LittleFS.open("/");
    if (root && root.isDirectory()) {
      File e = root.openNextFile();
      while (e) {
        const char* n = e.name();
        if (n && strstr(n, "wakes_")) {
          // Match ".sealed" as a suffix only. A substring test would
          // also match ".sealed.gz", which is a successful gzip — not
          // a stuck seal — and would block rotation forever.
          size_t nlen = strlen(n);
          if (nlen > 7 && strcmp(n + nlen - 7, ".sealed") == 0) {
            tprint("[WAKE] pending seal exists, skipping rotate: %s", n);
            e.close();
            root.close();
            return false;
          }
        }
        e.close();
        e = root.openNextFile();
      }
      root.close();
    }
  }

  // Rename current to .sealed
  if (!LittleFS.rename(_currentPath, sealed)) {
    tprint("[WAKE] rename failed %s", _currentPath);
    return false;
  }
  tprint("[WAKE] sealed as %s", sealed);

  // V4.37: gzip the sealed file alongside the original.
  // Keeps both — original survives if compression or upload fails.
  // Remove the original after gzip if LittleFS storage gets tight.
  File srcF = LittleFS.open(sealed, "r");
  size_t srcSize = srcF ? srcF.size() : 0;
  if (srcF) srcF.close();
  tprint("[WAKE] gzip check: src=%u", (unsigned)srcSize);
  if (srcSize > 1024) {
    char gzPath[80];
    snprintf(gzPath, sizeof(gzPath), "%s.gz", sealed);
    File srcGz = LittleFS.open(sealed, "r");
    File dstGz = LittleFS.open(gzPath, "w");
    size_t gzBytes = 0;
    if (srcGz && dstGz) {
      gzBytes = LZPacker::compress(&srcGz, srcGz.size(), &dstGz);
    }
    if (srcGz) srcGz.close();
    if (dstGz) dstGz.close();
    tprint("[WAKE] gzip result: %u", (unsigned)gzBytes);
    if (gzBytes > 0) {
      tprint("[WAKE] gzipped %s", gzPath);
    } else {
      tprint("[WAKE] gzip failed (returned 0)");
    }
  } else {
    tprint("[WAKE] too small to gzip (%u bytes)", (unsigned)srcSize);
  }

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
