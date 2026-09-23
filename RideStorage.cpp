
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

static void writeHeader(File& f, uint32_t startEpoch) {

  f.print("# fw=");
  f.println(BIKE_MATE_VERSION);

  time_t t = startEpoch;

  struct tm* ti = localtime(&t);

  char buf[80];

  snprintf(buf, sizeof(buf),
           "# ride_start=%lu (%04d-%02d-%02d %02d:%02d:%02d)",
           (unsigned long)startEpoch,
           ti->tm_year + 1900,
           ti->tm_mon + 1,
           ti->tm_mday,
           ti->tm_hour,
           ti->tm_min,
           ti->tm_sec);

  f.println(buf);

  f.println("epoch,lat,lon,volt,temp,state");
}

// ---- create ----

bool rideStorageCreate(uint32_t startEpoch) {

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
      startEpoch
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

// ---- close ----

void rideStorageClose() {

  if (_rideFile) {

    _rideFile.close();

    tprint(
        "[RIDE] closed %s",
        _currentFile
    );
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

const char* rideStorageCurrentFile() {

  return _currentFile;
}

