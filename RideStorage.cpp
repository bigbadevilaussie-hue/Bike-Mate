
// RideStorage.cpp — BIKE-MATE
//
// Manages date-keyed ride files in LittleFS.
//
// Ride lifecycle:
//   start  -> create a uniquely named ride file
//   running -> append rows
//   stop   -> close ride file
//   upload -> handled by DriveUpload.cpp
//
// Filename includes seconds so two rides started in the same minute
// cannot overwrite each other.

#define DEST_FS_USES_LITTLEFS
#include <ESP32-targz.h>
#include "RideStorage.h"

#include "Config.h"

#include <LittleFS.h>
#include <time.h>

extern void tprint(const char* fmt, ...);

// Currently open ride file
static File _rideFile;

static char _currentFile[40] = "";

static uint32_t _currentEpoch = 0;

// ---- filename builder ----

void rideStorageBuildFilename(uint32_t epoch, char* buf, size_t n) {

  setenv("TZ", "AEST-10", 1);
  tzset();

  time_t t = epoch;

  struct tm* ti = localtime(&t);

  snprintf(buf, n,
           "/ride_%04d%02d%02d%02d%02d%02d.csv",
           ti->tm_year + 1900,
           ti->tm_mon + 1,
           ti->tm_mday,
           ti->tm_hour,
           ti->tm_min,
           ti->tm_sec);
}

// ---- header writer ----

static void writeHeader(File& f, uint32_t startEpoch, float preRideVoltage) {

  f.print("# fw=");
  f.println(BIKE_MATE_VERSION);

  time_t t = startEpoch;

  struct tm* ti = localtime(&t);

  char buf[96];

  snprintf(buf, sizeof(buf),
           "# opened=%lu (%04d-%02d-%02d %02d:%02d:%02d)",
           (unsigned long)startEpoch,
           ti->tm_year + 1900,
           ti->tm_mon + 1,
           ti->tm_mday,
           ti->tm_hour,
           ti->tm_min,
           ti->tm_sec);

  f.println(buf);

  char pvbuf[48];
  snprintf(pvbuf, sizeof(pvbuf),
           "# pre_ride_volt=%.2f",
           preRideVoltage);
  f.println(pvbuf);

  extern int32_t rideStartLat_x1e7;
  extern int32_t rideStartLon_x1e7;
  if (rideStartLat_x1e7 != 0 || rideStartLon_x1e7 != 0) {
    char slbuf[64];
    snprintf(slbuf, sizeof(slbuf),
             "# start_loc=%.7f,%.7f",
             rideStartLat_x1e7 / 1e7,
             rideStartLon_x1e7 / 1e7);
    f.println(slbuf);
  }

  f.println("epoch,lat,lon,volt,temp,state");
}

// ---- create ----

bool rideStorageCreate(uint32_t startEpoch, float preRideVoltage) {

  if (_rideFile) {

    tprint("[RIDE] already open, closing");

    rideStorageClose();
  }

  rideStorageBuildFilename(
      startEpoch,
      _currentFile,
      sizeof(_currentFile)
  );

  _currentEpoch = startEpoch;

  _rideFile = LittleFS.open(
      _currentFile,
      "w"
  );

  if (!_rideFile) {

    tprint(
        "[RIDE] create failed %s",
        _currentFile
    );

    _currentFile[0] = 0;
    _currentEpoch = 0;

    return false;
  }

  writeHeader(
      _rideFile,
      startEpoch,
      preRideVoltage
  );

  tprint(
      "[RIDE] created %s",
      _currentFile
  );

  return true;
}

// ---- append row ----

bool rideStorageAppendRow(const RideRow& row) {

  if (!_rideFile) {

    tprint("[RIDE] no file open");

    return false;
  }

  char line[96];

  int n = snprintf(
      line,
      sizeof(line),
      "%lu,%ld,%ld,%.2f,%d,%d\n",
      (unsigned long)row.epoch,
      (long)row.lat_x1e7,
      (long)row.lon_x1e7,
      row.volt_x100 / 100.0,
      (int)row.temp,
      (int)row.state
  );

  if (n <= 0) {
    return false;
  }

  size_t written =
      _rideFile.write(
          (uint8_t*)line,
          n
      );

  return written == (size_t)n;
}

// ---- log close stamp ----
void rideStorageLogClose(uint32_t closeEpoch) {
  if (!_rideFile) return;
  time_t t = closeEpoch;
  struct tm* ti = localtime(&t);
  char buf[96];
  snprintf(buf, sizeof(buf),
           "# closed=%lu (%04d-%02d-%02d %02d:%02d:%02d)\n",
           (unsigned long)closeEpoch,
           ti->tm_year + 1900,
           ti->tm_mon + 1,
           ti->tm_mday,
           ti->tm_hour,
           ti->tm_min,
           ti->tm_sec);
  _rideFile.write((uint8_t*)buf, strlen(buf));
  tprint("[RIDE] closed stamp @ %lu", (unsigned long)closeEpoch);
}

// ---- close ----

void rideStorageClose() {

  if (_rideFile) {

    _rideFile.close();

    tprint(
        "[RIDE] closed %s",
        _currentFile
    );

    if (_currentFile[0] != 0) {
      File srcF = LittleFS.open(_currentFile, "r");
      size_t srcSize = srcF ? srcF.size() : 0;
      if (srcF) srcF.close();
      tprint("[RIDE] gzip check: src=%u", (unsigned)srcSize);
      // V5.27: gzip every file, no threshold.
      if (srcSize > 0) {
        char gzPath[64];
        snprintf(gzPath, sizeof(gzPath), "%s.gz", _currentFile);
        File srcGz = LittleFS.open(_currentFile, "r");
        File dstGz = LittleFS.open(gzPath, "w");
        size_t gzBytes = 0;
        if (srcGz && dstGz) {
          gzBytes = LZPacker::compress(&srcGz, srcGz.size(), &dstGz);
        }
        if (srcGz) srcGz.close();
        if (dstGz) dstGz.close();
        tprint("[RIDE] gzip result: %u", (unsigned)gzBytes);
        if (gzBytes > 0) {
          tprint("[RIDE] gzipped %s", gzPath);
          // V5.30: delete the raw after successful gzip. Was the
          // source of the orphan .csv pile.
          LittleFS.remove(_currentFile);
          tprint("[RIDE] removed raw %s", _currentFile);
        } else {
          tprint("[RIDE] gzip failed (returned 0)");
        }
      } else {
        tprint("[RIDE] empty file, skipping gzip");
      }
    }
  }

  _currentFile[0] = 0;
  _currentEpoch = 0;
}

// ---- delete ----

bool rideStorageDelete(uint32_t startEpoch) {

  char path[40];

  rideStorageBuildFilename(
      startEpoch,
      path,
      sizeof(path)
  );

  if (!LittleFS.exists(path)) {
    return false;
  }

  bool ok =
      LittleFS.remove(path);

  if (ok) {

    tprint(
        "[RIDE] deleted %s",
        path
    );
  }

  return ok;
}

// ---- enumerate ----

size_t rideStorageEnumerate(
    RideFileVisitor visitor
) {

  size_t count = 0;

  File root =
      LittleFS.open("/");

  if (!root ||
      !root.isDirectory()) {

    return 0;
  }

  File entry =
      root.openNextFile();

  while (entry) {

    const char* name =
        entry.name();

    if (name &&
        strncmp(
            name,
            "/ride_",
            6
        ) == 0) {

      if (visitor) {

        visitor(
            name,
            entry.size()
        );
      }

      count++;
    }

    entry =
        root.openNextFile();
  }

  root.close();

  return count;
}

// ---- exists ----

bool rideStorageExists(
    uint32_t startEpoch
) {

  char path[40];

  rideStorageBuildFilename(
      startEpoch,
      path,
      sizeof(path)
  );

  return LittleFS.exists(path);
}

// ---- current file ----

// V4.82: reopen the ride file after deep-sleep wake if a ride was in
// progress. Mirrors the create path but uses "a" mode so existing rows
// are preserved.
bool rideStorageReopen(uint32_t startEpoch) {
  if (startEpoch == 0) return false;

  char path[40];
  rideStorageBuildFilename(startEpoch, path, sizeof(path));

  if (!LittleFS.exists(path)) {
    tprint("[RIDE] reopen: file not found %s", path);
    return false;
  }

  if (_rideFile) {
    _rideFile.close();
  }
  _rideFile = LittleFS.open(path, "a");
  if (!_rideFile) {
    tprint("[RIDE] reopen: open failed %s", path);
    return false;
  }

  strncpy(_currentFile, path, sizeof(_currentFile) - 1);
  _currentFile[sizeof(_currentFile) - 1] = 0;
  _currentEpoch = startEpoch;
  return true;
}

const char* rideStorageCurrentFile() {

  return _currentFile;
}

