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
#include <base64.h>
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
  if (!dot) return;
  // V5.12: keep ".gz" so the upload name matches the content.
  // Previously "wakes_X.csv.sealed.gz" was truncated to "wakes_X.csv"
  // and the gzipped payload landed on Drive under a plain .csv name,
  // breaking the reports which expect text in .csv files.
  char* tail = dot + 7;  // strlen(".sealed")
  if (strcmp(tail, ".gz") == 0) {
    strcpy(dot, ".gz");
  } else {
    *dot = 0;
  }
}

// ---- POST one file ----
// ---- routing: prefix -> subfolder ----
static const char* ghSubfolder(const char* name) {
  if (strncmp(name, "wakes_", 6) == 0) return "WAKES";
  if (strncmp(name, "ride_",  5) == 0) return "RIDES";
  if (strncmp(name, "dyna_",  5) == 0) return "DYNA";
  if (strncmp(name, "bike_mate_", 10) == 0) return "BIN";
  return "";
}

// ---- URL-encode a single path segment ----
static String urlEncSegment(const String& s) {
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      char b[4];
      snprintf(b, sizeof(b), "%%%02X", (unsigned char)c);
      out += b;
    }
  }
  return out;
}

// ---- POST one file to GitHub Contents API ----
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

  // Read the whole file. Max sizes here are a few KB.
  uint8_t* buf = (uint8_t*)malloc(fileSize);
  if (!buf) {
    tprint("[UPLOAD] malloc %u failed", (unsigned)fileSize);
    f.close();
    return false;
  }
  size_t got = f.read(buf, fileSize);
  f.close();
  if (got != fileSize) {
    tprint("[UPLOAD] short read %u/%u", (unsigned)got, (unsigned)fileSize);
    free(buf);
    return false;
  }

  String b64 = base64::encode(buf, fileSize);
  free(buf);

  String name(uploadName);
  const char* sub = ghSubfolder(name.c_str());
  String path = (sub[0] ? String(sub) + "/" : String("")) + name;

  String json;
  json.reserve(b64.length() + 200);
  json  = "{\"message\":\"upload ";
  json += name;
  json += "\",\"content\":\"";
  json += b64;
  json += "\",\"branch\":\"";
  json += GH_BRANCH;
  json += "\"}";

  String url = "https://api.github.com/repos/";
  url += GH_REPO;
  url += "/contents/";
  url += urlEncSegment(path);

  tprint("[UPLOAD] PUT %s size=%u b64=%u",
         path.c_str(), (unsigned)fileSize, (unsigned)b64.length());

  HTTPClient http;
  http.setReuse(false);
  http.setTimeout(UPLOAD_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + GH_TOKEN);
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  http.addHeader("User-Agent", "bike-mate-esp32");
  http.addHeader("Content-Type", "application/json");

  int code = -1;
  for (int attempt = 1; attempt <= 3; attempt++) {
    code = http.PUT((uint8_t*)json.c_str(), json.length());
    if (code == 201 || code == 200) break;
    tprint("[UPLOAD] %s attempt %d code=%d", path.c_str(), attempt, code);
    if (code > 0) {
      String resp = http.getString();
      resp.trim();
      tprint("[UPLOAD] %s resp=%s", path.c_str(), resp.c_str());
    }
    http.end();
    delay(1000);
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + GH_TOKEN);
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("X-GitHub-Api-Version", "2022-11-28");
    http.addHeader("User-Agent", "bike-mate-esp32");
    http.addHeader("Content-Type", "application/json");
  }

  bool ok = (code == 201 || code == 200);
  tprint("[UPLOAD] %s %s (code=%d)",
         path.c_str(), ok ? "OK" : "FAIL", code);
  http.end();
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

      // V4.55: skip raw .sealed if a .sealed.gz exists for it
      String nameStr = String(name);
      if (nameStr.endsWith(".sealed")) {
        String gzPath = nameStr;
        if (!gzPath.startsWith("/")) gzPath = "/" + gzPath;
        gzPath += ".gz";
        if (LittleFS.exists(gzPath)) {
          entry.close();
          entry = root.openNextFile();
          continue;
        }
      }

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

  // V4.44: ensure TZ is set before building filenames. localtime()
  // returns UTC if tzset() hasn't run, and Rule 7 compares filenames.
  setenv("TZ", "AEST-10", 1);
  tzset();

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

      // V4.55: skip raw .csv if a .csv.gz exists for it
      String nameStr = String(name);
      if (nameStr.endsWith(".csv")) {
        String gzPath = nameStr;
        if (!gzPath.startsWith("/")) gzPath = "/" + gzPath;
        gzPath += ".gz";
        if (LittleFS.exists(gzPath)) {
          entry.close();
          entry = root.openNextFile();
          continue;
        }
      }

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

      // Rule 7: never upload the newest completed ride.
      // It stays on the ESP forever so the GUI can always pull it
      // via BLE. Only rides OLDER than newest_epoch are uploaded.
      // When a newer ride finishes, the previously-newest becomes
      // eligible and is uploaded on the next run.
      if (!skip) {
        extern Preferences prefs;
        prefs.begin("rides", true);
        uint32_t newest = prefs.getUInt("newest_epoch", 0);
        prefs.end();
        if (newest > 0) {
          char newestPath[48];
          rideStorageBuildFilename(newest, newestPath, sizeof(newestPath));
          const char* np = (newestPath[0] == '/') ? newestPath + 1 : newestPath;

          // V4.50: strip .gz so .csv.gz of the newest ride is skipped too
          char nStripped[64];
          strncpy(nStripped, n, sizeof(nStripped) - 1);
          nStripped[sizeof(nStripped) - 1] = 0;
          char* gz = strstr(nStripped, ".gz");
          if (gz) *gz = 0;

          tprint("[UPLOAD] Rule7: n='%s' np='%s' newest=%lu", nStripped, np, (unsigned long)newest);
          if (strcmp(nStripped, np) == 0) {
            skip = true;
            tprint("[UPLOAD] skipping newest ride %s", n);
          }
        } else {
          tprint("[UPLOAD] Rule7: newest_epoch is 0");
        }
      }

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

  uint32_t last4 = mostRecent4am();

  if (last4 == 0) return false;

  return getLastUploadEpoch() < last4;
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

  // V4.39: newest_epoch is NEVER cleared here. The newest completed
  // ride stays on the ESP permanently so the GUI can pull it via BLE.
  // Only older rides are uploaded and deleted. newest_epoch advances
  // naturally in closeRideLog() when a newer ride finishes.
  if (rideFails == 0) {
    tprint("[UPLOAD] upload ok, newest_epoch preserved");
  } else {
    tprint("[UPLOAD] ride upload failed, newest_epoch preserved");
  }

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
