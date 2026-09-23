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
RTC_DATA_ATTR bool lowBattMailLatched = false;

bool isCountingDown = false;
unsigned long countdownStartMillis = 0;
extern const int countdownSeconds = 30;
bool isArmingCountdown = false;
unsigned long armingStartMillis = 0;
extern const int armingSeconds = 10;
bool parkedTimerActive = false;
unsigned long parkedTimerStart = 0;
const unsigned long parkedDelayMs = 10000;
unsigned long lastPanicBeep = 0;
unsigned long lastTick = 0;
unsigned long lastPublish = 0;
unsigned long lastAdvRestart = 0;
unsigned long lastSecondMark = 0;

static unsigned long bootMillis = 0;
unsigned long lastDisconnectMillis = 0;

static bool firstTick = false;

volatile bool wifiActive = false;
volatile bool uploadRequested = false;

unsigned long lastUploadAttempt = 0;

RTC_DATA_ATTR static uint8_t uploadCycleCounter = 0;

// ---- GPS stub ----
bool gpsHasFix() {
#if GPS_STUB_ENABLED
  return true;
#else
  return false;
#endif
}

int32_t gpsLat_x1e7() {
#if GPS_STUB_ENABLED
  return (int32_t)(GPS_STUB_LAT * 1e7);
#else
  return 0;
#endif
}

int32_t gpsLon_x1e7() {
#if GPS_STUB_ENABLED
  return (int32_t)(GPS_STUB_LON * 1e7);
#else
  return 0;
#endif
}

uint8_t gpsSats() {
#if GPS_STUB_ENABLED
  return GPS_STUB_SATS;
#else
  return 0;
#endif
}

// ---- logging ----
void tprint(const char* fmt, ...) {
  unsigned long ms = millis();
  char ts[20];
  snprintf(ts, sizeof(ts), "[%02lu:%02lu:%02lu.%03lu] ",
           ms/3600000, (ms/60000)%60, (ms/1000)%60, ms%1000);
  Serial.print(ts);
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}

void tprint_verbose(const char* fmt, ...) {
#if DEBUG_VERBOSE
  unsigned long ms = millis();
  char ts[20];
  snprintf(ts, sizeof(ts), "[%02lu:%02lu:%02lu.%03lu] ",
           ms/3600000, (ms/60000)%60, (ms/1000)%60, ms%1000);
  Serial.print(ts);
  char buf[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
#endif
}

uint32_t currentEpoch() {
  if (macTimeEpoch == 0) return 0;
  return macTimeEpoch + (totalSeconds - secondsAtSync);
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
#if BENCH_MODE
  return "BENCH";
#else
  uint32_t ep = currentEpoch();
  if (ep == 0) return "DAY";
  time_t t = ep;
  struct tm* ti = localtime(&t);
  int h = ti->tm_hour;
  bool night = (h >= NIGHT_START_HOUR || h < NIGHT_END_HOUR);
  return night ? "NIGHT" : "DAY";
#endif
}

uint32_t currentWakeMs() {
#if BENCH_MODE
  return WAKE_BENCH_MS;
#else
  uint32_t ep = currentEpoch();
  if (ep == 0) return WAKE_DAY_MS;
  time_t t = ep;
  struct tm* ti = localtime(&t);
  int h = ti->tm_hour;
  bool night = (h >= NIGHT_START_HOUR || h < NIGHT_END_HOUR);
  return night ? WAKE_NIGHT_MS : WAKE_DAY_MS;
#endif
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
  bool base = !inPanic && !engineWasRunning && !accState &&
              !isCountingDown && !isArmingCountdown && !isLogging &&
              !alarmSequenceActive && !lowBattBeepActive;
  if (!base) return false;

  if (otaRequest) return false;
  if (wifiActive) return false;
  if (uploadRequested) return false;
  if (millis() - bootMillis < 5000UL) return false;

  if (isActuallyConnected()) {
    if (millis() - bootMillis < 60000UL) return false;
    return true;
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
  const char* mode = currentWakeMode();
  uint32_t wakeMs = currentWakeMs();
  uint32_t wakeSec = wakeMs / 1000;
  digitalWrite(STATUS_LED_PIN, LOW);
  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  tprint("[SLEEP] mode=%s V=%.2f conn=%d wake=%lus",
         mode, latestBatteryVoltage,
         isActuallyConnected() ? 1 : 0, wakeSec);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)wakeMs * 1000ULL);
  esp_deep_sleep_start();
}

void updateStateTransitions(unsigned long now) {
  float V = latestBatteryVoltage;

  if (!engineWasRunning && !isCountingDown && !inPanic &&
      V >= V_RESTING_MIN && V <= V_RESTING_MAX) {
    lastRestingVoltage = V;
  }

  if (!inPanic && V < V_PANIC_ENTER) {
    inPanic = true;
    lastPanicBeep = now;
    lowBattBeepActive = true;
    lowBattBeepStep = 0;
    lowBattBeepRemaining = 5;
    lowBattBeepTimer = now;
    tprint("[STATE] PANIC enter %.2f", V);
    wakeLoggerForceWrite();
    if (engineWasRunning || accState || isLogging) {
      engineWasRunning = false;
      accState = false;
      isCountingDown = false;
      isArmingCountdown = true;
      armingStartMillis = millis();
    }
  } else if (inPanic && V > V_PANIC_EXIT) {
    inPanic = false;
    tprint("[STATE] PANIC exit %.2f", V);
  }

  bool atRest = !engineWasRunning && !isCountingDown && !accState &&
                !inPanic && !isArmingCountdown;

  if (atRest && V < WARN_EMAIL_VOLTAGE && !lowBattMailLatched) {
    lowVoltLoops++;
    if (lowVoltLoops >= LOW_VOLT_LOOPS_REQUIRED) {
      if (sendLowBatteryAlert(V, WARN_EMAIL_VOLTAGE)) {
        lowBattMailLatched = true;
        lowVoltLoops = 0;
        tprint("[MAIL] low batt alert sent %.2f", V);
      } else {
        tprint("[MAIL] send failed, will retry");
      }
    } else {
      tprint_verbose("[MAIL] low volt loop %d/%d", lowVoltLoops, LOW_VOLT_LOOPS_REQUIRED);
    }
  } else if (V >= WARN_EMAIL_VOLTAGE || !atRest) {
    if (lowVoltLoops > 0) {
      lowVoltLoops = 0;
      tprint_verbose("[MAIL] low volt loops reset");
    }
  }

  if (V >= WARN_RECOVER_VOLTAGE && lowBattMailLatched) {
    lowBattMailLatched = false;
    tprint("[MAIL] latch cleared (recovered %.2f)", V);
  }

  if (V >= V_RUNNING_ENTER) {
    parkedTimerActive = false;
    if (!engineWasRunning && !isCountingDown) {
      engineWasRunning = true;
      isArmingCountdown = false;
      alarmSequenceActive = true; alarmStep = 0; alarmStepTimer = now;
      isCountingDown = true;
      countdownStartMillis = millis();
      ridePreVoltage = lastRestingVoltage;
      tprint("[STATE] engine start %.2f preV=%.2f", V, ridePreVoltage);
      wakeLoggerForceWrite();
    }
  }

  if (V < V_RUNNING_EXIT && engineWasRunning && !isCountingDown) {
    if (!parkedTimerActive) {
      parkedTimerActive = true;
      parkedTimerStart = now;
    }
  } else if (V >= V_RUNNING_EXIT) {
    parkedTimerActive = false;
  }

  if (isCountingDown &&
      (millis() - countdownStartMillis) / 1000 >= countdownSeconds) {
    isCountingDown = false;
    accState = true;
    if (!isLogging) startRideLog();
    tprint("[STATE] settle complete ACC ON %.2f", V);
    wakeLoggerForceWrite();
  }

  if (parkedTimerActive && (now - parkedTimerStart) >= parkedDelayMs) {
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
    if (isLogging) closeRideLog();
    tprint("[STATE] arming complete");
    wakeLoggerForceWrite();

#if UPLOAD_ENABLED && BENCH_MODE
    uploadCycleCounter++;
    if (uploadCycleCounter >= 2) {
      uploadCycleCounter = 0;
      uploadRequested = true;
      tprint("[UPLOAD] bench trigger set (2 armings)");
    }
#endif
  }
}

void doStateWork(unsigned long now) {
  float V = latestBatteryVoltage;

  if (isLogging) writeRideRow();

  if (inPanic && !lowBattBeepActive) {
    if (now - lastPanicBeep >= 120000UL) {
      lastPanicBeep = now;
      lowBattBeepActive = true;
      lowBattBeepStep = 0;
      lowBattBeepRemaining = 5;
      lowBattBeepTimer = now;
    }
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

#if BENCH_MODE
  if (uploadRequested) {
    uploadDue = true;
    uploadRequested = false;
    tprint("[UPLOAD] bench trigger consumed");
  }
#else
  if (driveUploadShouldRun() && currentEpoch() > 0 &&
      (lastUploadAttempt == 0 ||
       millis() - lastUploadAttempt > UPLOAD_COOLDOWN_MS)) {
    uploadDue = true;
  }
#endif

  if (uploadDue && !otaRequest && !wifiActive) {
    lastUploadAttempt = millis();
    tprint("[UPLOAD] ====== ENTERING UPLOAD MODE ======");

    extern char wifiMessage[24];
    snprintf(wifiMessage, sizeof(wifiMessage), "Starting");

    wifiActive = true;
    wakeLoggerPause();

    uint32_t nowEp = currentEpoch();
    if (nowEp > 0) {
      wakeLoggerForceRotate(nowEp);
    }

    driveUploadPerform();

    delay(5000);
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);

    wakeLoggerResume();
    wifiActive = false;
  }
#endif  // UPLOAD_ENABLED
}

void setup() {
  Serial.begin(115200);
  delay(300);
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const char* causeStr =
      (cause == ESP_SLEEP_WAKEUP_TIMER) ? "timer" :
      (cause == ESP_SLEEP_WAKEUP_UNDEFINED) ? "cold boot" : "other";
  tprint("=================================================");
  tprint("  BIKE-MATE V%s", BIKE_MATE_VERSION);
#if BENCH_MODE
  tprint("  BENCH_MODE=1  wake=30s/30s");
#else
  tprint("  BENCH_MODE=0  day=300s night=600s");
#endif
  tprint("=================================================");
  tprint("[WAKE] cause=%d (%s)", (int)cause, causeStr);
  setenv("TZ", "AEST-10", 1); tzset();
  cycleCount++;
  tprint("[BOOT] cycleCount = %lu", cycleCount);
  bool coldBoot = (cause == ESP_SLEEP_WAKEUP_UNDEFINED);
  if (coldBoot) {
    macTimeEpoch = 0;
    totalSeconds = 0;
    secondsAtSync = 0;
    engineWasRunning = false;
    accState = false;
    inPanic = false;
    isLogging = false;
    currentRowCount = 0;
    lastRestingVoltage = 0.0;
    lowVoltLoops = 0;
    lowBattMailLatched = false;
    uploadCycleCounter = 0;
    tprint("cold boot: state reset");
  } else {
    totalSeconds += currentWakeMs() / 1000;
    tprint_verbose("timer wake: totalSeconds = %lu", (unsigned long)totalSeconds);
  }

  prefs.begin("ota", true);
  bool otaPending = prefs.getBool("pending", false);
  prefs.end();
  if (otaPending) {
    tprint("[OTA] ====== POST-UPDATE COLD BOOT DETECTED ======");
    prefs.begin("ota", false);
    prefs.putBool("pending", false);
    prefs.end();
    tprint("[OTA] sending confirmation email");
    bool mailOk = sendMail("Bike-Mate: OTA complete",
                           String("Firmware updated successfully.\n\n") +
                           "New version: " + BIKE_MATE_VERSION + "\n" +
                           "Device: bike-mate\n");
    if (mailOk) tprint("[OTA] confirmation email sent");
    else        tprint("[OTA] confirmation email FAILED");
  }

  if (!wakeLoggerInit()) {
    tprint_verbose("[BOOT] WakeLogger init deferred (no epoch yet)");
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
    tprint("splash...");
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
  tprint("[ADC] raw=%d battery=%.2fV ntc=%d temp=%.1fC",
         latestRawVoltage, latestBatteryVoltage,
         latestNtcRaw, latestTemperatureC);
  bleInit();
  pServer->getAdvertising()->start();
  tprint_verbose("BLE advertising started");
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

  readSensors();
  updateStateTransitions(now);
  digitalWrite(ACC_LED_PIN, accState ? HIGH : LOW);
  updateBeeps(now);
  updateOLED_EdgeTriggered();

  if (now - lastAdvRestart >= ADV_RESTART_MS) {
    lastAdvRestart = now;
    if (pServer && !isActuallyConnected()) {
      pServer->getAdvertising()->start();
    }
  }
  if (isActuallyConnected() && now - lastPublish >= 2000) {
    lastPublish = now;
    publishBLE();
  }

  if (pushRequested && isActuallyConnected() && !pushInProgress) {
    pushRequested = false;
    pushNewSlots(pushGuiEpoch);
  }

  if (firstTick || (now - lastTick >= TICK_MS)) {
    firstTick = false;
    lastTick = now;
    doStateWork(now);
  }

  if (shouldSleep()) {
    goToSleep();
  }

  delay(50);
}