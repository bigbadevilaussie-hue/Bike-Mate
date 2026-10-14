// build stamp V5.37 (BLE upload)
// bike_mate.ino
// Main coordinator: state machine, sleep, WiFi trigger, upload trigger.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <stdarg.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <time.h>
#include <Preferences.h>
#include <esp_arduino_version.h>
#include <LittleFS.h>

#include "Config.h"
#include "Settings.h"
#include "Sensors.h"
#include "Buzzer.h"
#include "RideLogger.h"
#include "RideStorage.h"
#include "WakeLogger.h"
#include "BleManager.h"
#include "DisplayManager.h"
#include "WifiMail.h"
#include "OtaManager.h"
#include "DriveUpload.h"
#include "WifiManager.h"
#include "GpsModule.h"
#include "SerialBuffer.h"
#include "WebServer.h"

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#error "Bike-Mate requires Arduino ESP32 core 2.x (2.0.17)"
#endif

Preferences prefs;

RTC_DATA_ATTR uint32_t macTimeEpoch = 0;
RTC_DATA_ATTR uint32_t secondsAtSync = 0;
RTC_DATA_ATTR uint32_t totalSeconds = 0;
RTC_DATA_ATTR bool engineWasRunning = false;
RTC_DATA_ATTR bool accState = false;
RTC_DATA_ATTR bool inPanic = false;
RTC_DATA_ATTR unsigned long cycleCount = 0;
RTC_DATA_ATTR float lastRestingVoltage = 0.0;
RTC_DATA_ATTR uint8_t lowVoltLoops = 0;
RTC_DATA_ATTR uint8_t panicLatchCount = 0;
static bool engineStartCapture = false;
static unsigned long engineStartCaptureMs = 0;
static unsigned long lastCaptureRow = 0;
RTC_DATA_ATTR bool lowBattMailLatched = false;

// V4.76: maintenance mode request. Set by BLE callback, consumed in
// doStateWork() on the next wake, cleared when the session exits.
// Same pattern as otaRequest.
RTC_DATA_ATTR bool maintRequest = false;

// V5.44: button-triggered 120s awake window (field mode only).
RTC_DATA_ATTR bool riderReadyActive = false;
RTC_DATA_ATTR unsigned long riderReadyStart = 0;

// V5.37.6: manual upload trigger from OSA. Set by the {"cmd":"upload"}
// BLE write, consumed by doStateWork on the next pass.
RTC_DATA_ATTR bool bleUploadNowRequested = false;

bool isCountingDown = false;
unsigned long countdownStartMillis = 0;
extern const int countdownSeconds = 30;
bool isArmingCountdown = false;
unsigned long armingStartMillis = 0;
extern const int armingSeconds = 10;
bool parkedTimerActive = false;
unsigned long parkedTimerStart = 0;
const unsigned long parkedDelayMs = 0;
unsigned long lastPanicBeep = 0;
unsigned long lastTick = 0;
unsigned long lastPublish = 0;
unsigned long lastAdvRestart = 0;
unsigned long lastSecondMark = 0;

static unsigned long bootMillis = 0;
unsigned long maintStartMs = 0;  // V4.88: non-static, read by OLED
unsigned long lastDisconnectMillis = 0;

static bool firstTick = false;

volatile bool wifiActive = false;

unsigned long lastUploadAttempt = 0;


// GPS functions now in GpsModule.cpp

// ---- logging ----
void tprint(const char* fmt, ...) {
  unsigned long ms = millis();
  char ts[20];
  snprintf(ts, sizeof(ts), "[%02lu:%02lu:%02lu.%03lu] ",
           ms/3600000, (ms/60000)%60, (ms/1000)%60, ms%1000);
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  // V4.77: build a single line with timestamp + trailing newline so
  // the ring buffer serves complete lines to /serial-raw. Matches the
  // shape Fan-Mate's log_print() produces.
  char line[232];
  snprintf(line, sizeof(line), "%s%s\n", ts, buf);

  Serial.print(line);
  serialBufWrite(line);
}

void tprint_verbose(const char* fmt, ...) {
#if DEBUG_VERBOSE
  unsigned long ms = millis();
  char ts[20];
  snprintf(ts, sizeof(ts), "[%02lu:%02lu:%02lu.%03lu] ",
           ms/3600000, (ms/60000)%60, (ms/1000)%60, ms%1000);
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  char line[232];
  snprintf(line, sizeof(line), "%s%s\n", ts, buf);

  Serial.print(line);
  serialBufWrite(line);
#endif
}

// V5.28: monotonic epoch floor. Bench-mode clock syncs (BLE from GUI,
// Opal LAN on maint entry, /clock POST) can pull macTimeEpoch backward.
// That produced out-of-order wake rows. The floor clamps currentEpoch()
// so it never returns a value lower than the highest one already
// returned this session. Reset on cold boot (RTC RAM wipe). No effect
// in the field where no backward sync happens.
RTC_DATA_ATTR uint32_t _epochFloor = 0;

uint32_t currentEpoch() {
  if (macTimeEpoch == 0) return 0;
  uint32_t e = macTimeEpoch + (totalSeconds - secondsAtSync);
  if (e < _epochFloor) e = _epochFloor;
  _epochFloor = e;
  return e;
}

// V5.18: persist the clock to NVS so a cold boot doesn't lose
// the epoch. Without this, a ride started after a cold boot has
// currentEpoch()==0, startRideLog() bails, and the ride is lost.
static void clockSave() {
  if (macTimeEpoch == 0) return;
  Preferences p;
  p.begin("clock", false);
  p.putUInt("epoch", macTimeEpoch);
  p.putUInt("at_total", secondsAtSync);
  p.end();
}

static bool clockRestore() {
  Preferences p;
  p.begin("clock", true);
  uint32_t e = p.getUInt("epoch", 0);
  uint32_t t = p.getUInt("at_total", 0);
  p.end();
  if (e < 1700000000UL || e > 4102444800UL) return false;
  macTimeEpoch = e;
  secondsAtSync = t;
  tprint("[CLOCK] restored from NVS epoch=%lu", (unsigned long)e);
  return true;
}

void formatTime12h_buf(uint32_t e, char* out, size_t n) {
  if (e == 0) { snprintf(out, n, "--:--"); return; }
  time_t t = e;
  struct tm* ti = localtime(&t);
  int h = ti->tm_hour, m = ti->tm_min;
  const char* ap = (h < 12) ? "am" : "pm";
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  snprintf(out, n, "%d:%02d%s", h12, m, ap);
}

const char* currentWakeMode() {
  if (config.benchMode) return "BENCH";
  uint32_t ep = currentEpoch();
  if (ep == 0) return "DAY";
  time_t t = ep;
  struct tm* ti = localtime(&t);
  int h = ti->tm_hour;
  bool offPeak = (config.offPeakStart <= config.offPeakEnd)
      ? (h >= config.offPeakStart && h < config.offPeakEnd)
      : (h >= config.offPeakStart || h < config.offPeakEnd);
  bool peak = (h >= config.peakStart && h < config.peakEnd);
  if (offPeak) return "OFFPEAK";
  if (peak) return "PEAK";
  return "DAY";
}

uint32_t currentWakeMs() {
  if (config.benchMode) return WAKE_BENCH_MS;
  uint32_t ep = currentEpoch();
  if (ep == 0) return (uint32_t)config.dayWakeMin * 60000UL;
  time_t t = ep;
  struct tm* ti = localtime(&t);
  int h = ti->tm_hour;
  bool offPeak = (config.offPeakStart <= config.offPeakEnd)
      ? (h >= config.offPeakStart && h < config.offPeakEnd)
      : (h >= config.offPeakStart || h < config.offPeakEnd);
  bool peak = (h >= config.peakStart && h < config.peakEnd);
  uint8_t m = offPeak ? config.offPeakWakeMin
            : peak    ? config.peakWakeMin
            :           config.dayWakeMin;
  if (m < 1)  m = 1;
  if (m > 30) m = 30;
  return (uint32_t)m * 60000UL;
}

void wakeFlash() {
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(150);
  digitalWrite(STATUS_LED_PIN, LOW);
  delay(250);
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(150);
  digitalWrite(STATUS_LED_PIN, LOW);
}

const char* currentStateString() {
  if (inPanic) return "PANIC";
  if (engineWasRunning) return "RUNNING";
  if (isCountingDown) return "RUNNING";
  if (accState) return "RUNNING";
  return "MONITOR";
}

static bool shouldSleep() {
  // V4.72: PANIC is now sleep-eligible. PANIC wakes every scheduled
  // interval, alarms, mails, and returns to sleep. No death spiral.
  bool base = !engineWasRunning && !accState &&
              !isCountingDown && !isArmingCountdown && !isLogging &&
              !alarmSequenceActive && !lowBattBeepActive;
  if (!base) return false;

  if (otaRequest) return false;
  if (wifiActive) return false;
  if (serverIsRunning()) return false;
  if (maintRequest) return false;

  // V5.44: Rider for Rider hold window.
  if (riderReadyActive) {
    unsigned long elapsed = millis() - riderReadyStart;
    if (elapsed < RIDER_READY_MS) {
      return false;
    }
    riderReadyActive = false;
    tprint("[RIDER] ended after %lus", elapsed / 1000UL);
  }
  // V5.40: bench mode holds the full 30 s window from bootMillis.
  // Field mode sleeps as soon as work is done.
  if (config.benchMode) {
    if (millis() - bootMillis < 30000UL) return false;
  } else {
    if (millis() - bootMillis < 5000UL) return false;
  }

  if (millis() - lastDisconnectMillis < 3000UL) return false;

  return true;
}

static void goToSleep() {
  unsigned long advStart = millis();
  while (millis() - advStart < MONITOR_ADV_MS) {
    if (isActuallyConnected()) break;
    delay(50);
  }
  if (isActuallyConnected()) {
    unsigned long pushStart = millis();
    while (millis() - pushStart < 5000) {
      if (pushRequested) {
        pushRequested = false;
        pushNewSlots(pushGuiEpoch);
        break;
      }
      delay(50);
    }
  }
  // V5.07: last-chance check. The BLE callback runs on the BT stack
  // task, so maintRequest can be set while we're inside the wait
  // loops above. If it's set now, abort the sleep and return to the
  // main loop so the maint block runs on this wake, not the next.
  if (maintRequest) {
    tprint("[SLEEP] abort - maint requested during sleep prep");
    return;
  }

  clockSave();

  const char* mode = currentWakeMode();
  uint32_t wakeMs = currentWakeMs();
  uint32_t wakeSec = wakeMs / 1000;
  digitalWrite(STATUS_LED_PIN, LOW);
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  // V5.42: removed. The loop already increments totalSeconds once
  // per awake second. This line counted the same awake window a
  // second time, doubling the counter and racing the clock ahead
  // of real time. The floor in currentEpoch() then locked the epoch
  // at the raced value, and every seal collided on the same filename.

  tprint("[SLEEP] mode=%s V=%.2f conn=%d wake=%lus",
         mode, latestBatteryVoltage,
         isActuallyConnected() ? 1 : 0, wakeSec);

  // V5.44: button handling on the way to sleep.
  //  - wait for release so a held button doesn't insta-wake
  //  - if it's still held after the wait, don't arm GPIO wake
  //  - field mode only
  {
    unsigned long t0 = millis();
    while (digitalRead(WAKE_BTN_PIN) == LOW && millis() - t0 < 10000UL) {
      delay(10);
    }
    delay(200);   // debounce settle
    bool btnHeld = (digitalRead(WAKE_BTN_PIN) == LOW);
    if (!config.benchMode && !btnHeld) {
      esp_deep_sleep_enable_gpio_wakeup(
          1ULL << WAKE_BTN_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
      tprint("[RIDER] GPIO wake armed");
    } else if (btnHeld) {
      tprint("[RIDER] button held at sleep, GPIO wake disabled");
    }
  }

  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)wakeMs * 1000ULL);
  esp_deep_sleep_start();
}

void updateStateTransitions(unsigned long now) {
  float V = latestBatteryVoltage;

  if (!engineWasRunning && !isCountingDown && !inPanic &&
      V >= (config.monitorNormal_mv / 1000.0f) && V <= V_RESTING_MAX) {
    lastRestingVoltage = V;
  }

  // V4.51: PANIC enter moved to doStateWork() - only checked on wake.
  // PANIC exit still here for fast recovery.
  if (inPanic && V > ((config.monitorPanic_mv + WARN_RECOVER_GAP_MV) / 1000.0f)) {
    inPanic = false;
    panicLatchCount = 0;
    tprint("[STATE] PANIC exit %.2f", V);
  }

  bool atRest = !engineWasRunning && !isCountingDown && !accState &&
                !inPanic && !isArmingCountdown;

  float warnEmailV = config.monitorWarning_mv / 1000.0f;
  float warnRecoverV = (config.monitorWarning_mv + WARN_RECOVER_GAP_MV) / 1000.0f;

  if (atRest && V < warnEmailV && !lowBattMailLatched) {
    lowVoltLoops++;
    if (lowVoltLoops >= LOW_VOLT_LOOPS_REQUIRED) {
      // V4.51: latch on any attempt - success or failure. Otherwise a
      // disabled mail path (MAIL_ENABLED 0) or a rate-limit retries
      // every loop iteration.
      lowBattMailLatched = true;
      lowVoltLoops = 0;
      if (sendLowBatteryAlert(V, warnEmailV)) {
        tprint("[MAIL] low batt alert sent %.2f", V);
      } else {
        tprint("[MAIL] low batt alert attempted (send failed)");
      }
    } else {
      tprint_verbose("[MAIL] low volt loop %d/%d", lowVoltLoops, LOW_VOLT_LOOPS_REQUIRED);
    }
  } else if (V >= warnEmailV || !atRest) {
    if (lowVoltLoops > 0) {
      lowVoltLoops = 0;
      tprint_verbose("[MAIL] low volt loops reset");
    }
  }

  if (V >= warnRecoverV && lowBattMailLatched) {
    lowBattMailLatched = false;
    tprint("[MAIL] latch cleared (recovered %.2f)", V);
  }

  if (V >= (config.runningEnter_mv / 1000.0f)) {
    parkedTimerActive = false;
    if (!engineWasRunning && !isCountingDown) {
      engineWasRunning = true;
      isArmingCountdown = false;
      riderReadyActive = false;   // V5.45: rider arrived, end ready window
      tprint("[RIDER] engine start, ready window cleared");
      alarmSequenceActive = true; alarmStep = 0; alarmStepTimer = now;
      isCountingDown = true;
      countdownStartMillis = millis();
      ridePreVoltage = lastRestingVoltage;
      if (gpsHasFix()) {
        setRideStartLocation(gpsLat_x1e7(), gpsLon_x1e7());
        tprint("[STATE] ride start loc %.7f,%.7f",
               gpsLat_x1e7() / 1e7, gpsLon_x1e7() / 1e7);
      }
      tprint("[STATE] engine start %.2f preV=%.2f", V, ridePreVoltage);
      wakeLoggerForceWrite();
      engineStartCapture = true;
      engineStartCaptureMs = millis();
      lastCaptureRow = 0;
    }
  }

  if (V < (config.runningExit_mv / 1000.0f) && engineWasRunning && !isCountingDown) {
    if (!parkedTimerActive) {
      parkedTimerActive = true;
      parkedTimerStart = now;
    }
  } else if (V >= (config.runningExit_mv / 1000.0f)) {
    parkedTimerActive = false;
  }

  if (isCountingDown &&
      (millis() - countdownStartMillis) / 1000 >= countdownSeconds) {
    isCountingDown = false;
    accState = true;
    if (!isLogging) startRideLog();
    tprint("[STATE] settle complete ACC ON %.2f", V);
    wakeLoggerForceWrite();
    wakeLoggerPause();
  }

  if (parkedTimerActive && (now - parkedTimerStart) >= parkedDelayMs) {
    if (isLogging) closeRideLog();   // V4.55: ride ends at engine stop
    engineWasRunning = false;
    accState = false;
    isArmingCountdown = true;
    armingStartMillis = millis();
    parkedTimerActive = false;
    tprint("[STATE] engine stop -> arming %.2f", V);
    wakeLoggerForceWrite();
  }

  if (isArmingCountdown &&
      (millis() - armingStartMillis) / 1000 >= armingSeconds) {
    isArmingCountdown = false;
    alarmSequenceActive = true; alarmStep = 0; alarmStepTimer = millis();
    // V4.55: closeRideLog moved to engine stop - ride ends when engine stops
    tprint("[STATE] arming complete");
    wakeLoggerResume();
    wakeLoggerForceWrite();

  }
}

void doStateWork(unsigned long now, bool wasWake) {
  float V = latestBatteryVoltage;

  // V4.51: PANIC check ONLY on wake (wasWake = true).
  // Not on TICK_MS. Not during rides. Not during transitions.
  // Latch counter survives deep sleep. Day: 3 wakes, Night: 4 wakes.
  bool panicCheckAllowed = wasWake &&
                           !engineWasRunning && !accState &&
                           !isCountingDown && !isArmingCountdown;

  if (panicCheckAllowed && !inPanic) {
    bool isNight = strcmp(currentWakeMode(), "NIGHT") == 0;
    uint8_t required = isNight ? 4 : 3;

    float panicExitV = (config.monitorPanic_mv + WARN_RECOVER_GAP_MV) / 1000.0f;
    if (V < panicExitV) {
      panicLatchCount++;
    } else {
      panicLatchCount = 0;
    }

    if (panicLatchCount >= required) {
      tprint("[PANIC] latch=%d/%d FIRE V=%.2f", panicLatchCount, required, V);
      inPanic = true;
      panicLatchCount = 0;
      lastPanicBeep = now;
      lowBattBeepActive = true;
      lowBattBeepStep = 0;
      lowBattBeepRemaining = 5;
      lowBattBeepTimer = now;
      tprint("[STATE] PANIC enter %.2f after %d wakes", V, required);
      wakeLoggerForceWrite();

      // V4.72: PANIC mail attempts on every wake. Latches bypassed.
      // The wake interval is the natural throttle.
      if (sendLowBatteryAlert(V, config.monitorPanic_mv / 1000.0f)) {
        tprint("[MAIL] PANIC alert sent %.2f", V);
      } else {
        tprint("[MAIL] PANIC send failed");
      }
    } else {
      tprint("[PANIC] latch=%d/%d V=%.2f", panicLatchCount, required, V);
    }
  } else if (wasWake && inPanic) {
    tprint("[PANIC] already inPanic V=%.2f", V);
  }

  // V4.72: beep once per PANIC wake. Device is awake for only a few
  // seconds, so this fires on every wake and the beep sequence plays
  // out before sleep. No 2-minute gate needed.
  // V4.73: beep only once per wake. doStateWork() can fire more than
  // once inside a single wake if TICK_MS elapses while still awake.
  static bool panicBeepThisWake = false;
  if (wasWake) panicBeepThisWake = false;
  if (inPanic && !panicBeepThisWake && !lowBattBeepActive) {
    panicBeepThisWake = true;
    lowBattBeepActive = true;
    lowBattBeepStep = 0;
    lowBattBeepRemaining = 5;
    lowBattBeepTimer = now;
    tprint("[PANIC] beep");
  }

  wakeLoggerSetLocation(gpsLat_x1e7(), gpsLon_x1e7(), gpsSats());

  publishBLE();

  wakeLoggerTick();

  tprint("[SENSOR] V=%.2f raw=%d ntc=%d temp=%.1fC",
         latestBatteryVoltage, latestRawVoltage,
         latestNtcRaw, latestTemperatureC);

  tprint_verbose("[TICK] V=%.2f -> %s | log=%d rows=%d conn=%d acc=%d led=%d pre=%.2f ota=%d",
                 V, currentStateString(),
                 isLogging ? 1 : 0, currentRowCount,
                 isActuallyConnected() ? 1 : 0,
                 accState ? 1 : 0,
                 (digitalRead(ACC_LED_PIN) == HIGH) ? 1 : 0,
                 lastRestingVoltage,
                 otaRequest ? 1 : 0);

  // ---- OTA (priority) ----
  if (otaRequest) {
    // V4.71: close any open ride cleanly before OTA reboots the device
    if (isLogging) {
      tprint("[OTA] closing ride before update");
      closeRideLog();
    }
    tprint("[OTA] ====== ENTERING OTA MODE ======");
    wifiActive = true;
    wakeLoggerPause();
    bool ok = otaPerformUpdate(otaUrl, otaSize, otaMd5);
    wakeLoggerResume();
    wifiActive = false;
    if (!ok) {
      tprint("[OTA] ====== OTA FAILED ======");
      otaRequest = false;
    }
  }

  if (otaRequest && otaStartMillis > 0 &&
      (millis() - otaStartMillis) > OTA_TIMEOUT_MS) {
    tprint("[OTA] timeout, clearing flag");
    otaRequest = false;
  }

  // ---- Drive upload ----
#if UPLOAD_ENABLED
  bool uploadDue = false;

  bool scheduledUpload = driveUploadShouldRun();
  bool manualUpload = bleUploadNowRequested;

  if ((scheduledUpload || manualUpload) && currentEpoch() > 0 &&
           (lastUploadAttempt == 0 ||
            millis() - lastUploadAttempt > UPLOAD_COOLDOWN_MS)) {
    uploadDue = true;
  }

  if (uploadDue && !otaRequest && !wifiActive) {
    lastUploadAttempt = millis();

    // V5.37: auto upload runs over BLE. Bike wakes at 04:00, OSA is
    // already connected, sealed files go out over the existing link.
    // Manual upload from maint still uses WiFi via driveUploadPerform.
    wakeLoggerSealManual(currentEpoch());
    reconcileLittleFS();

    extern bool bleSendFile(const char* path, const char* uploadName);
    extern bool isActuallyConnected();

    if (!isActuallyConnected()) {
      tprint("[UPLOAD] auto: no BLE link, skipping");
    } else {
      int sent = 0;
      int found = 0;
      File root = LittleFS.open("/");
      if (root && root.isDirectory()) {
        File e = root.openNextFile();
        while (e) {
          String n = String(e.name());
          size_t sz = e.size();
          e.close();
          bool isWakeGz = n.startsWith("wakes_") && n.endsWith(".gz");
          bool isRideGz = n.startsWith("ride_")  && n.endsWith(".gz");
          if ((isWakeGz || isRideGz) && sz > 0) {
            found++;
            String path = n.startsWith("/") ? n : "/" + n;
            tprint("[UPLOAD] auto: found %s (%u bytes)", n.c_str(), (unsigned)sz);
            if (bleSendFile(path.c_str(), n.c_str())) {
              // V5.37.8: no delete on send. bleSendFile returning true
              // only means the chunks were queued, not that OSA wrote
              // to disk. Files stay on LittleFS until an ack path lands.
              sent++;
            } else {
              tprint("[UPLOAD] auto: send FAILED for %s", n.c_str());
            }
            delay(200);
          }
          e = root.openNextFile();
        }
        root.close();
      }
      tprint("[UPLOAD] auto: %d sealed, %d sent over BLE", found, sent);
    }

    // V5.37.6: clear the manual trigger. Only fires once per request.
    bleUploadNowRequested = false;

    uint32_t ep = currentEpoch();
    if (ep > 0) {
      Preferences p;
      p.begin(NVS_NAMESPACE_UPLOAD, false);
      p.putUInt("last_epoch", ep);
      p.end();
    }
  }
#endif  // UPLOAD_ENABLED
}

void setup() {
  // V5.20: suppress the benign IDF wifi-teardown warning that
  // fires on every WiFi down: 'timeout when WiFi un-init'.
  esp_log_level_set("wifi", ESP_LOG_NONE);
  Serial.begin(115200);
  delay(300);
  serialBufInit();
  gpsModuleInit();
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const char* causeStr =
      (cause == ESP_SLEEP_WAKEUP_TIMER) ? "timer" :
      (cause == ESP_SLEEP_WAKEUP_UNDEFINED) ? "cold boot" : "other";
  setenv("TZ", "AEST-10", 1); tzset();
  settingsLoad();   // V5.34: load NVS before the banner so it prints real values
  tprint("=================================================");
  tprint("  BIKE-MATE V%s", BIKE_MATE_VERSION);
  if (config.benchMode) {
    tprint("  BENCH_MODE=1  wake=30s/30s");
  } else {
    tprint("  BENCH_MODE=0  op=%u-%u/%um pk=%u-%u/%um day=%um",
           config.offPeakStart, config.offPeakEnd, config.offPeakWakeMin,
           config.peakStart, config.peakEnd, config.peakWakeMin,
           config.dayWakeMin);
  }
  tprint("=================================================");
  tprint("[WAKE] cause=%d (%s)", (int)cause, causeStr);
  cycleCount++;
  tprint("[BOOT] cycleCount = %lu", cycleCount);
  bool coldBoot = (cause == ESP_SLEEP_WAKEUP_UNDEFINED);
  if (coldBoot) {
    if (!clockRestore()) {
      macTimeEpoch = 0;
      totalSeconds = 0;
      secondsAtSync = 0;
    }
    engineWasRunning = false;
    accState = false;
    inPanic = false;
    isLogging = false;
    currentRowCount = 0;
    lastRestingVoltage = 0.0;
    lowVoltLoops = 0;
    lowBattMailLatched = false;
    panicLatchCount = 0;
  } else {
    totalSeconds += currentWakeMs() / 1000;

    // V5.44: GPIO wake from the button. Field only.
    if (cause == ESP_SLEEP_WAKEUP_GPIO &&
        !config.benchMode &&
        !riderReadyActive) {
      riderReadyActive = true;
      riderReadyStart = millis();
      tprint("[RIDER] ready, %lus window", RIDER_READY_MS / 1000UL);
    }
    tprint_verbose("timer wake: totalSeconds = %lu", (unsigned long)totalSeconds);
  }

  prefs.begin("ota", true);
  bool otaPending = prefs.getBool("pending", false);
  prefs.end();
  tprint("[OTA] boot read pending=%d", otaPending ? 1 : 0);
  if (otaPending) {
    prefs.begin("ota", false);
    prefs.putBool("pending", false);
    prefs.end();
    tprint("[OTA] pending flag cleared, version now V%s", BIKE_MATE_VERSION);

    // V4.79: after an OTA, force the bike into maintenance mode on
    // this boot so the serial page comes up immediately. Lets the
    // GUI confirm the new firmware without waiting a wake cycle.
    maintRequest = true;
    tprint("[MAINT] post-OTA boot, forcing maint on this wake");
  }

  if (!wakeLoggerInit()) {
    tprint_verbose("[BOOT] WakeLogger init deferred (no epoch yet)");
  }

  // V4.73: clear stale .sealed files. This must run AFTER wakeLoggerInit()
  // because LittleFS.begin() is inside wakeLoggerInit(). Running it earlier
  // meant the scan silently saw an unmounted filesystem and never deleted
  // anything, which is why "pending seal exists" fired on every cycle.
  {
    File root = LittleFS.open("/");
    if (root && root.isDirectory()) {
      File e = root.openNextFile();
      while (e) {
        String n = String(e.name());
        if (n.endsWith(".sealed")) {
          String path = n.startsWith("/") ? n : "/" + n;
          e.close();
          LittleFS.remove(path);
          tprint("[TEMP] deleted pending %s", path.c_str());
          e = root.openNextFile();
          continue;
        }
        e.close();
        e = root.openNextFile();
      }
      root.close();
    }
  }

  loadRideState();

  {
    prefs.begin("rides", true);
    uint32_t ne = prefs.getUInt("newest_epoch", 0);
    prefs.end();

    if (ne > 0 && !rideStorageExists(ne)) {
      prefs.begin("rides", false);
      prefs.putUInt("newest_epoch", 0);
      prefs.end();
      tprint("[BOOT] cleared stale newest_epoch=%lu", (unsigned long)ne);
    }
  }

  pinMode(ACC_LED_PIN, OUTPUT); digitalWrite(ACC_LED_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(STATUS_LED_PIN, OUTPUT); digitalWrite(STATUS_LED_PIN, LOW);
  pinMode(WAKE_BTN_PIN, INPUT_PULLUP);
  ledcSetup(0, 2000, 8);
  ledcAttachPin(BUZZER_PIN, 0);
  ledcWrite(0, 0);
  analogSetAttenuation(ADC_11db);
  delay(50);
  analogRead(VOLTAGE_PIN);
  analogRead(THERMISTOR_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay(); display.display();
  if (coldBoot) {
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(16, 12); display.print("Bike-Mate");
    display.setTextSize(1);
    display.setCursor(48, 40); display.print("V");
    display.print(BIKE_MATE_VERSION);
    display.display();
    delay(SPLASH_MS);
    display.clearDisplay(); display.display();
  }
  readSensors();
  // V4.88: if this wake is going to enter maintenance mode, do NOT
  // bring BLE up. V4.84 made the BLE link sticky across sleep, and on
  // the C3 (core 2.0.17, bad antenna) a BLE session that survives a
  // wake leaves the coexistence arbiter holding the radio — WiFi init
  // succeeds but association never progresses (status stuck at 0).
  // Skipping bleInit() entirely on maint-bound wakes gives WiFi a
  // clean radio, matching the pre-V4.84 flow.
  if (!maintRequest) {
    bleInit();
    pServer->getAdvertising()->start();
    tprint_verbose("BLE advertising started");
  } else {
    tprint("[MAINT] maint-bound wake: BLE skipped, radio free for WiFi");
  }
  lastPublish = 0;
  lastAdvRestart = millis();
  lastSecondMark = millis();
  lastTick = millis();
  bootMillis = millis();
  lastDisconnectMillis = 0;
  firstTick = true;
  wakeFlash();
  tprint("[BOOT] ready");
}

void loop() {
  if (wifiActive) {
    drawOLED();
    delay(100);
    return;
  }

  unsigned long now = millis();
  if (now - lastSecondMark >= 1000) {
    lastSecondMark = now;
    totalSeconds++;
  }

  gpsModuleTick();

  // V5.40: GPS debug print gated on accState. The MOSFET powers
  // the module only after the 30 s settle, so anything before that
  // is cached zeroes from a dead UART. Hides the parked-mode spam.
  static unsigned long lastGpsPrint = 0;
  if (accState && millis() - lastGpsPrint > 5000) {
    lastGpsPrint = millis();
    tprint_verbose("[GPS] fix=%d sats=%d spd=%.1f lat=%ld lon=%ld utc=%s hdg=%.1f alt=%.1f",
           gpsHasFix() ? 1 : 0, gpsSats(), gpsSpeed_kmh(),
           (long)gpsLat_x1e7(), (long)gpsLon_x1e7(),
           gpsTimeUTC(), gpsCourseDeg(), gpsAltitude_m());
  }

  // V5.34: [LOOP] alive removed - the [GPS] tick is the heartbeat.
  static unsigned long lastStage = 0;
  lastStage = millis();
  readSensors();
  if (millis() - lastStage > 100UL) tprint("[LOOP] readSensors took %lums", millis() - lastStage);

  lastStage = millis();
  updateStateTransitions(now);
  if (millis() - lastStage > 100UL) tprint("[LOOP] updateStateTransitions took %lums", millis() - lastStage);

  digitalWrite(ACC_LED_PIN, accState ? HIGH : LOW);

  lastStage = millis();
  updateBeeps(now);
  if (millis() - lastStage > 100UL) tprint("[LOOP] updateBeeps took %lums", millis() - lastStage);

  lastStage = millis();
  updateOLED_EdgeTriggered();
  if (millis() - lastStage > 100UL) tprint("[LOOP] updateOLED took %lums", millis() - lastStage);

  // V4.55: only restart advertising on the transition from connected
  // to disconnected. Repeated start() calls cause BT_HCI op=0x2008/0x2009
  // status=0x7 spam. isAdvertising() isn't in this ESP32 core version.
  static bool wasConnected = false;
  if (now - lastAdvRestart >= ADV_RESTART_MS) {
    lastAdvRestart = now;
    bool conn = isActuallyConnected();
    if (pServer && !conn && wasConnected) {
      pServer->getAdvertising()->start();
    }
    wasConnected = conn;
  }
  if (isActuallyConnected() && now - lastPublish >= 2000) {
    lastPublish = now;
    publishBLE();
  }

  if (pushRequested && isActuallyConnected() && !pushInProgress) {
    pushRequested = false;
    pushNewSlots(pushGuiEpoch);
  }

  // V4.41: ride log writes on its own 5s timer.
  static unsigned long lastRideLog = 0;
  if (isLogging && (now - lastRideLog >= (LOG_INTERVAL_SEC * 1000UL))) {
    lastRideLog = now;
    writeRideRow();
  }

  // V4.73: force BLE disconnect if a client has held the link past
  // BLE_MAX_CONN_MS. Prevents the Mac GUI from pinning the device awake.
  {
    static unsigned long bleConnectedSince = 0;
    static bool bleWasConnected = false;
    bool conn = isActuallyConnected();
    if (conn && !bleWasConnected) bleConnectedSince = millis();
    if (!conn && bleWasConnected) bleConnectedSince = 0;
    if (conn && bleConnectedSince > 0 &&
        (millis() - bleConnectedSince) > BLE_MAX_CONN_MS) {
      tprint("[BLE] hard timeout, disconnecting");
      pServer->disconnect(pServer->getConnId());
      bleConnectedSince = 0;
    }
    bleWasConnected = conn;
  }

  // V4.57: engine-start capture - 100ms wake rows for 5s after engine start
  // V4.61: use a fresh millis() value inside the block. The loop-level
  // 'now' is a stale snapshot taken before updateStateTransitions() ran,
  // so now - engineStartCaptureMs can wrap around (engineStartCaptureMs
  // is set later in the same iteration, after a LittleFS write, making it
  // larger than the stale 'now'). That wraparound caused the >= 5000UL
  // check to fire on the first iteration, killing the capture window.
  if (engineStartCapture) {
    static unsigned long lastActiveLog = 0;
    unsigned long captureNow = millis();
#if DEBUG_CAPTURE
    if (captureNow - lastActiveLog > 500) {
      lastActiveLog = captureNow;
      tprint("[CAPTURE] active flag=%d elapsed=%lu",
             (int)engineStartCapture,
             (unsigned long)(captureNow - engineStartCaptureMs));
    }
#endif
    if (captureNow - engineStartCaptureMs >= 5000UL) {
      engineStartCapture = false;
    } else if (captureNow - lastCaptureRow >= 100UL) {
      lastCaptureRow = captureNow;
      extern void wakeLoggerForceWriteQuiet();
      wakeLoggerForceWriteQuiet();
      wakeLoggerForceWrite(1);
    }
  }

  // V5.37.9: fire doStateWork immediately when a manual BLE
  // upload is queued, not on the next TICK_MS boundary.
  // Field mode otherwise waits a full 300/600 s wake.
  if (firstTick || bleUploadNowRequested ||
      (now - lastTick >= TICK_MS)) {
    bool wasWake = firstTick;
    firstTick = false;
    lastTick = now;
    doStateWork(now, wasWake);
  }

  // V4.76: maintenance mode. Enter on next wake after BLE request.
  //   Exit: /maint/off hit, or MAINT_MAX_MS elapsed.
  if (maintRequest && !serverIsRunning()) {
    wifiActive = true;
    wakeLoggerPause();
    if (wifiBringUp()) {
      serverSetup();
      // V5.07: clock fetch is NOT on the critical path. Opal can
      // stall 3+ s, NTP can take 8+ s. Server starts immediately,
      // clock syncs after. If it fails, RTC is used and logged.
      clockBringUp();
      maintStartMs = millis();
      tprint("[MAINT] active, cap %lus", MAINT_MAX_MS / 1000UL);
      bool engineStartBail = false;
      static unsigned long lastMaintOled = 0;
      extern volatile bool uploadNowRequested;
      static unsigned long lastMaintHb = 0;
      while (serverIsRunning()) {
        unsigned long _t0 = millis();
        serverLoop();
        unsigned long _dt = millis() - _t0;
        if (_dt > 500UL) tprint("[MAINT] serverLoop took %lums", _dt);
        delay(20);

        if (millis() - lastMaintHb >= 5000UL) {
          lastMaintHb = millis();
          tprint("[MAINT] alive %lus remaining",
                 (MAINT_MAX_MS - (millis() - maintStartMs)) / 1000UL);
        }

        if (uploadNowRequested) {
          uploadNowRequested = false;
          tprint("[UPLOAD] /upload consumed, running now");
          uint32_t nowEp = currentEpoch();
          if (nowEp > 0) wakeLoggerForceRotate(nowEp);
          driveUploadPerform(true, "forced");
        }

        // V4.87: keep the OLED awake for the whole maint session.
        // Without this the screen stays blank after the prior
        // goToSleep() powered it down, so an unattended bike in
        // maint looks identical to a sleeping bike.
        if (millis() - lastMaintOled >= 1000UL) {
          lastMaintOled = millis();
          display.ssd1306_command(SSD1306_DISPLAYON);
          drawOLED();
        }

        if (maintOffRequested) {
          tprint("[MAINT] exiting (user)");
          break;
        }
        if ((millis() - maintStartMs) > MAINT_MAX_MS) {
          tprint("[MAINT] exiting (timeout)");
          break;
        }

        // V4.87: sample voltage every 500 ms. If the engine is
        // starting, bail out immediately so WiFi drops and the
        // normal state machine takes over. Protects the prime
        // directive: an unattended maint session must not race a
        // ride start and hold WiFi up for 15 min.
        static unsigned long lastMaintSense = 0;
        if (millis() - lastMaintSense >= 500UL) {
          lastMaintSense = millis();
          unsigned long _st = millis();
          readSensors();
          if (millis() - _st > 200UL) tprint("[MAINT] readSensors took %lums", millis() - _st);
          float mv = latestBatteryVoltage;
          if (mv >= (config.runningEnter_mv / 1000.0f)) {
            tprint("[MAINT] engine start detected (%.2fV), exiting",
                   mv);
            engineStartBail = true;
            break;
          }
        }
      }
      maintOffRequested = false;
      serverStop();
      wifiBringDown();
      wakeLoggerResume();
      wifiActive = false;
      if (engineStartBail) {
        tprint("[MAINT] ====== MAINTENANCE ENDED (engine start) ======");
      } else {
        tprint("[MAINT] ====== MAINTENANCE ENDED ======");
      }
    } else {
      tprint("[MAINT] wifi bring-up FAILED");
      wifiBringDown();
      wakeLoggerResume();
      wifiActive = false;
    }
    maintRequest = false;
  }

  // V4.72: hard max-awake timeout. Any stuck state that keeps us awake
  // past MAX_AWAKE_MS gets force-slept. OTA is exempt — force-sleeping
  // mid-flash bricks the firmware, worse than staying awake.
  // V4.75: maintenance mode is also exempt — it has its own cap.
  // V4.81: guard only fires when the device is idle. Rides, arming,
  // engine-run, and alarms are legitimate reasons to stay awake past
  // MAX_AWAKE_MS. Previously the guard killed mid-ride logging.
  bool inActiveWork = engineWasRunning || accState ||
                      isCountingDown || isArmingCountdown ||
                      isLogging || alarmSequenceActive ||
                      lowBattBeepActive;

  // V4.85: the guard measures idle time, not total awake time.
  // When we transition from busy to idle, reset the guard clock so
  // queued work (upload, mail, whatever) has a fresh window to run.
  static bool guardWasActive = false;
  static unsigned long guardIdleSince = 0;
  if (inActiveWork && !guardWasActive) {
    guardIdleSince = millis();
    tprint("[GUARD] activity start, idle timer reset");
  } else if (!inActiveWork && guardWasActive) {
    guardIdleSince = millis();
    tprint("[GUARD] activity ended, idle window begins");
  }
  guardWasActive = inActiveWork;

  // V4.87: hard exempt - even if inActiveWork misclassifies for one
  // iteration, never force-sleep mid-ride.
  bool hardExempt = isLogging || engineWasRunning || accState;

  if (!otaRequest && !serverIsRunning() && !inActiveWork &&
      !hardExempt && guardIdleSince > 0 &&
      (millis() - guardIdleSince) > MAX_AWAKE_MS) {
    tprint("[GUARD] idle timeout (%lus), forcing sleep",
           (unsigned long)((millis() - guardIdleSince) / 1000UL));
    goToSleep();
  }

  if (shouldSleep()) {
    goToSleep();
  }

  delay(50);
}