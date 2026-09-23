// DriveUpload.cpp — BIKE-MATE V3.58
// Uploads wake files and ride files to Google Drive via Apps Script.
// V3.58: Rule 2/6 — newest completed ride uploads. newest_epoch cleared after upload.
// V3.34: driveUploadShouldRun() returns false in bench mode (triggered externally).
// V3.31: sets wifiMessage during upload for OLED display.
// V3.30: 302 treated as success. Local files deleted after upload.
// V3.28: LittleFS path slash fix.

#include "DriveUpload.h"
#include "WakeLogger.h"
#include "RideStorage.h"
#include "Config.h"
#include "WifiManager.h"
#include "DisplayManager.h"

#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>

extern void tprint(const char* fmt, ...);
extern uint32_t currentEpoch();
extern unsigned long cycleCount;
extern Preferences prefs;

// ---- upload filename buffer ----
// Static storage avoids placing ~3 KB on the ESP32-C3 stack.
#define MAX_UPLOAD_FILES 64
static char uploadNames[MAX_UPLOAD_FILES][48];

// ---- URL encode ----
static String urlEncode(const String& s) {
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

// ---- ensure LittleFS path has leading slash ----
static void makeFullPath(const char* in, char* out, size_t n) {
  if (in[0] == '/') {
    strncpy(out, in, n - 1);
    out[n - 1] = 0;
  } else {
    snprintf(out, n, "/%s", in);
  }
}

// ---- strip .sealed suffix ----
static void stripSealed(const char* in, char* out, size_t n) {
  strncpy(out, in, n - 1);
  out[n - 1] = 0;
  char* dot = strstr(out, ".sealed");
  if (dot) *dot = 0;
}

// ---- POST one file ----
static bool postFile(const char* localPath, const char* uploadName) {
  char fullPath[64];
  makeFullPath(localPath, fullPath, sizeof(fullPath));

  File f = LittleFS.open(fullPath, "r");
  if (!f) {
    tprint("[UPLOAD] cannot open %s", fullPath);
    return false;
  }

  size_t fileSize = f.size();

  if (fileSize == 0) {
    tprint("[UPLOAD] %s empty, skipping", fullPath);
    f.close();
    return true;
  }

  String content;
  content.reserve(fileSize + 16);

  while (f.available()) {
    content += (char)f.read();
  }

  f.close();

  String body = "filename=" + urlEncode(String(uploadName)) +
                "&data=" + urlEncode(content);

  tprint("[UPLOAD] POST %s size=%u body=%u",
         uploadName,
         (unsigned)fileSize,
         (unsigned)body.length());

  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(UPLOAD_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.begin(UPLOAD_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Content-Length", String(body.length()));

  int code = http.POST((uint8_t*)body.c_str(), body.length());
  bool ok = false;

  if (code == 302) {
    String resp = http.getString();
    ok = true;

    tprint("[UPLOAD] %s OK (302)", uploadName);

    http.end();

  } else if (code == 200) {
    String resp = http.getString();
    resp.trim();

    ok = resp.startsWith("OK");

    tprint("[UPLOAD] %s code=200 resp=%s",
           uploadName,
           resp.c_str());

    http.end();

  } else if (code > 0) {
    String resp = http.getString();
    resp.trim();

    tprint("[UPLOAD] %s code=%d resp=%s",
           uploadName,
           code,
           resp.c_str());

    http.end();

  } else {
    tprint("[UPLOAD] %s http=%d",
           uploadName,
           code);

    http.end();
  }

  return ok;
}

// ---- wake enumeration ----
static bool isCurrentWakeFile(const char* name, const char* current) {
  if (!current || current[0] == 0) return false;

  const char* n = (name[0] == '/') ? name + 1 : name;
  const char* c = (current[0] == '/') ? current + 1 : current;

  return strcmp(n, c) == 0;
}

static int uploadAllWakeFiles(const char* currentWakeFile, int* okCount) {
  extern char wifiMessage[24];

  snprintf(wifiMessage, sizeof(wifiMessage), "Wakes");

  int fails = 0;
  *okCount = 0;
  int nameCount = 0;

  // ---- Phase 1: collect filenames only ----
  // No uploads or deletes while directory handles are open.
  File root = LittleFS.open("/");

  if (!root || !root.isDirectory()) {
    return 1;
  }

  File entry = root.openNextFile();

  while (entry && nameCount < MAX_UPLOAD_FILES) {
    const char* name = entry.name();

    if (name &&
        (strncmp(name, "/wakes_", 7) == 0 ||
         strncmp(name, "wakes_", 6) == 0)) {

      if (!isCurrentWakeFile(name, currentWakeFile)) {
        strncpy(uploadNames[nameCount],
                name,
                sizeof(uploadNames[nameCount]) - 1);

        uploadNames[nameCount]
            [sizeof(uploadNames[nameCount]) - 1] = 0;

        nameCount++;
      }
    }

    // Important: close the current directory entry
    // before opening the next one.
    entry.close();

    entry = root.openNextFile();
  }

  entry.close();
  root.close();

  // ---- Phase 2: upload + delete ----
  // Directory and entry handles are now completely closed.
  for (int i = 0; i < nameCount; i++) {

    const char* rawName =
        (uploadNames[i][0] == '/')
        ? uploadNames[i] + 1
        : uploadNames[i];

    char cleanName[64];

    stripSealed(rawName,
                cleanName,
                sizeof(cleanName));

    bool ok = postFile(uploadNames[i], cleanName);

    wakeLoggerLogUpload(
        cleanName,
        ok,
        ok ? "" : "upload failed"
    );

    if (ok) {
      char fullPath[64];

      makeFullPath(
          uploadNames[i],
          fullPath,
          sizeof(fullPath)
      );

      LittleFS.remove(fullPath);

      (*okCount)++;

    } else {
      fails++;
    }
  }

  return fails;
}

// ---- ride enumeration ----
static int uploadAllRideFiles(int* okCount) {
  extern char wifiMessage[24];

  snprintf(wifiMessage, sizeof(wifiMessage), "Rides");

  int fails = 0;
  *okCount = 0;
  int nameCount = 0;

  const char* openRide = rideStorageCurrentFile();

  // ---- Phase 1: collect filenames only ----
  File root = LittleFS.open("/");

  if (!root || !root.isDirectory()) {
    return 1;
  }

  File entry = root.openNextFile();

  while (entry && nameCount < MAX_UPLOAD_FILES) {

    const char* name = entry.name();

    if (name &&
        (strncmp(name, "/ride_", 6) == 0 ||
         strncmp(name, "ride_", 5) == 0)) {

      bool skip = false;

      const char* n =
          (name[0] == '/')
          ? name + 1
          : name;

      // Rule 2: never upload the currently OPEN ride file.
      if (openRide && openRide[0]) {

        const char* o =
            (openRide[0] == '/')
            ? openRide + 1
            : openRide;

        if (strcmp(n, o) == 0) {
          skip = true;
        }
      }

      // Rule 6: completed rides are always upload-eligible.
      // (V3.58: removed the "newest completed ride" skip.)

      if (!skip) {

        strncpy(uploadNames[nameCount],
                name,
                sizeof(uploadNames[nameCount]) - 1);

        uploadNames[nameCount]
            [sizeof(uploadNames[nameCount]) - 1] = 0;

        nameCount++;
      }
    }

    // Important: close the current directory entry
    // before opening the next one.
    entry.close();

    entry = root.openNextFile();
  }

  entry.close();
  root.close();

  // ---- Phase 2: upload + delete ----
  // No directory handles remain open.
  for (int i = 0; i < nameCount; i++) {

    const char* n =
        (uploadNames[i][0] == '/')
        ? uploadNames[i] + 1
        : uploadNames[i];

    bool ok =
        postFile(uploadNames[i], n);

    wakeLoggerLogUpload(
        n,
        ok,
        ok ? "" : "upload failed"
    );

    if (ok) {

      char fullPath[64];

      makeFullPath(
          uploadNames[i],
          fullPath,
          sizeof(fullPath)
      );

      LittleFS.remove(fullPath);

      (*okCount)++;

    } else {
      fails++;
    }
  }

  return fails;
}

// ---- trigger ----
static uint32_t getLastUploadEpoch() {
  prefs.begin(NVS_NAMESPACE_UPLOAD, true);

  uint32_t v =
      prefs.getUInt("last_epoch", 0);

  prefs.end();

  return v;
}

static void setLastUploadEpoch(uint32_t epoch) {
  prefs.begin(NVS_NAMESPACE_UPLOAD, false);

  prefs.putUInt("last_epoch", epoch);

  prefs.end();
}

static uint32_t mostRecent4am() {
  uint32_t ep = currentEpoch();

  if (ep == 0) return 0;

  time_t t = ep;

  struct tm* ti = localtime(&t);

  struct tm target = *ti;

  target.tm_hour = UPLOAD_HOUR_LOCAL;
  target.tm_min = 0;
  target.tm_sec = 0;

  time_t target_epoch =
      mktime(&target);

  if (target_epoch > t) {
    target_epoch -= 86400;
  }

  return (uint32_t)target_epoch;
}

// V3.34: bench mode is triggered externally via uploadRequested flag.
// driveUploadShouldRun() only handles field mode (04:00).
bool driveUploadShouldRun() {

#if !UPLOAD_ENABLED
  return false;
#endif

#if BENCH_MODE
  return false;
#else
  uint32_t last4 = mostRecent4am();

  if (last4 == 0) return false;

  return getLastUploadEpoch() < last4;
#endif
}

// ---- main ----
bool driveUploadPerform() {
  extern char wifiMessage[24];

  tprint("[UPLOAD] ====== ENTERING UPLOAD MODE ======");

  display.ssd1306_command(SSD1306_DISPLAYON);
  display.clearDisplay();
  drawUploadScreen();
  display.display();

  wakeLoggerLogUploadStart();

  if (!wifiBringUp()) {

    wakeLoggerLogUpload(
        "wifi",
        false,
        "connect failed"
    );

    wakeLoggerLogUploadDone(0, 1);

    tprint("[UPLOAD] ====== FAILED (WiFi) ======");

    snprintf(
        wifiMessage,
        sizeof(wifiMessage),
        "WiFi fail"
    );

    return false;
  }

  int wakeOk = 0;
  int rideOk = 0;

  int wakeFails =
      uploadAllWakeFiles(
          wakeLoggerCurrentFile(),
          &wakeOk
      );

  int rideFails =
      uploadAllRideFiles(
          &rideOk
      );

  int totalFails =
      wakeFails + rideFails;

  int totalOk =
      wakeOk + rideOk;

  wakeLoggerLogUploadDone(
      totalOk,
      totalFails
  );

  wifiBringDown();

  // V3.58: clear newest_epoch after upload. Any completed ride
  // eligible for upload has now been uploaded (and deleted on 302).
  // Clearing this stops push chasing a deleted file on the next wake.
  prefs.begin("rides", false);
  prefs.putUInt("newest_epoch", 0);
  prefs.end();

  uint32_t ep = currentEpoch();

  if (ep > 0) {
    setLastUploadEpoch(ep);
  }

  bool success =
      (totalFails == 0);

  tprint(
      "[UPLOAD] ====== %s (ok=%d fail=%d) ======",
      success ? "OK" : "PARTIAL",
      totalOk,
      totalFails
  );

  snprintf(
      wifiMessage,
      sizeof(wifiMessage),
      success ? "Done" : "Partial"
  );

  return success;
}
