#include "DisplayManager.h"
#include "Config.h"
#include "OtaManager.h"

// V4.90: forward-declare to avoid pulling <WiFi.h> in via WebServer.h
bool serverIsRunning();

extern float latestBatteryVoltage;
extern float latestTemperatureC;
extern bool inPanic;
extern bool isCountingDown;
extern bool isArmingCountdown;
extern bool accState;
extern bool engineWasRunning;
extern bool isLogging;
extern unsigned long countdownStartMillis;
extern unsigned long armingStartMillis;
extern uint32_t rideStartEpoch;
extern volatile bool otaRequest;

extern const int countdownSeconds;
extern const int armingSeconds;

extern const char* currentStateString();
extern uint32_t currentEpoch();
extern void formatTime12h_buf(uint32_t e, char* out, size_t n);

extern volatile bool wifiActive;
extern char wifiMessage[24];

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1); // 128x32
char lastOLEDState[48] = "";

// ---- GPS — use the real functions from bike_mate.ino ----
extern bool gpsHasFix();
extern uint8_t gpsSats();

// ---- glyphs ----
static void drawRecDot(int16_t x, int16_t y, bool on) {
  if (!on) {
    display.drawCircle(x, y, 2, SSD1306_WHITE);
    return;
  }
  bool blink = ((millis() / 500) % 2) == 0;
  if (blink) display.fillCircle(x, y, 2, SSD1306_WHITE);
  else       display.drawCircle(x, y, 2, SSD1306_WHITE);
}

static void drawGpsBox(int16_t x, int16_t y, bool hasFix) {
  if (hasFix) display.fillRect(x, y, 5, 5, SSD1306_WHITE);
  else        display.drawRect(x, y, 5, 5, SSD1306_WHITE);
}

// ---- screens ----

extern bool gpsHasFix();
extern float gpsSpeed_kmh();
extern uint8_t gpsSats();


void drawUploadScreen() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 8);
  display.print("Maintenance");
  display.drawLine(20, 20, 108, 20, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(24, 28);
  display.print("Uploading");
}

static void drawBleUploadScreen() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(48, 8);
  display.print("BLE");
  display.setTextSize(1);
  display.setCursor(28, 24);
  display.print("Uploading...");
}

static void drawMaintScreen() {
  extern unsigned long maintStartMs;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(32, 0);
  display.print("MAINT");

  extern char wifiMessage[24];
  char msg[24];
  snprintf(msg, sizeof(msg), "%s", wifiMessage);
  int iw = strlen(msg) * 6;
  if (iw > SCREEN_W - 4) iw = SCREEN_W - 4;
  display.setTextSize(1);
  display.setCursor((SCREEN_W - iw) / 2, 16);
  display.print(msg);

  unsigned long now = millis();
  unsigned long elapsed = (now >= maintStartMs) ? (now - maintStartMs) : 0;
  long rem = (long)((MAINT_MAX_MS - elapsed) / 1000UL);
  if (rem < 0) rem = 0;
  char rb[20];
  snprintf(rb, sizeof(rb), "%lds left", rem);
  int rw = strlen(rb) * 6;
  display.setCursor((SCREEN_W - rw) / 2, 24);
  display.print(rb);
}

static void drawFwUpdateScreen() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(16, 0);
  display.print("Firmware Update");

  char verBuf[24];
  if (otaVersion[0]) snprintf(verBuf, sizeof(verBuf), "Ver %s", otaVersion);
  else snprintf(verBuf, sizeof(verBuf), "Ver --");
  display.setTextSize(2);
  int vw = strlen(verBuf) * 12;
  display.setCursor((SCREEN_W - vw) / 2, 12);
  display.print(verBuf);

  display.setTextSize(1);
  char status[24];
  if (otaStage == OTA_STAGE_DOWNLOAD) snprintf(status, sizeof(status), "Downloading...");
  else if (otaStage == OTA_STAGE_VERIFY) snprintf(status, sizeof(status), "Verifying...");
  else if (otaStage == OTA_STAGE_FLASH) snprintf(status, sizeof(status), "Flashing...");
  else if (otaStage == OTA_STAGE_REBOOT) snprintf(status, sizeof(status), "Reboot in %ds", otaRebootCountdown);
  else snprintf(status, sizeof(status), "Starting...");

  int sw = strlen(status) * 6;
  display.setCursor((SCREEN_W - sw) / 2, 24);
  display.print(status);
}

static void drawPanicScreen() {
  display.fillRect(0, 0, SCREEN_W, SCREEN_H, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(24, 0); display.print("LOW BATTERY");
  display.setTextSize(3);
  display.setCursor(16, 12);
  display.print(latestBatteryVoltage, 1);
  display.print("V");
}

static void drawCountdownScreen() {
  int rem = countdownSeconds - ((millis() - countdownStartMillis) / 1000);
  if (rem < 0) rem = 0;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print("ENGINE START");
  display.setTextSize(3);
  display.setCursor(44, 10); display.print(rem); display.print("s");
}

static void drawArmingScreen() {
  int rem = armingSeconds - ((millis() - armingStartMillis) / 1000);
  if (rem < 0) rem = 0;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print("ENGINE STOP");
  display.setTextSize(3);
  display.setCursor(44, 10); display.print(rem); display.print("s");
}

static void drawRunningScreen() {
  char tb[12];
  formatTime12h_buf(currentEpoch(), tb, sizeof(tb));
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(8, 0);
  display.print("REC");
  drawRecDot(2, 4, isLogging);
  drawGpsBox(72, 1, gpsHasFix());
  display.setCursor(82, 0);
  display.print("GPS ");
  display.print(gpsSats());

  char spd[8];
  if (gpsHasFix()) {
    int kph = (int)(gpsSpeed_kmh() + 0.5f);
    if (kph < 2) kph = 0;
    snprintf(spd, sizeof(spd), "%d", kph);
  } else {
    snprintf(spd, sizeof(spd), "--");
  }
  display.setTextSize(3);
  int slen = strlen(spd);
  int sx = (SCREEN_W - slen * 18) / 2;
  if (sx < 0) sx = 0;
  display.setCursor(sx, 10);
  display.print(spd);

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(latestBatteryVoltage, 1);
  display.print("V");
  display.setCursor(48, 24);
  display.print((int)latestTemperatureC);
  display.print("C");
  display.setCursor(88, 24);
  display.print(tb);
}

static void drawRiderReadyScreen() {
  extern bool riderReadyActive;
  extern unsigned long riderReadyStart;

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  const char* l1 = "RIDER";
  int w1 = strlen(l1) * 6;
  display.setCursor((SCREEN_W - w1) / 2, 0);
  display.print(l1);

  long elapsed_ms = (long)(millis() - riderReadyStart);
  long remaining = ((long)RIDER_READY_MS - elapsed_ms) / 1000L;
  if (remaining < 0) remaining = 0;
  if (remaining > (long)(RIDER_READY_MS / 1000UL)) remaining = (long)(RIDER_READY_MS / 1000UL);
  char cb[12];
  snprintf(cb, sizeof(cb), "%lds", remaining);
  int cw = strlen(cb) * 12;
  display.setTextSize(2);
  display.setCursor((SCREEN_W - cw) / 2, 8);
  display.print(cb);

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print(latestBatteryVoltage, 1);
  display.print("V");
  char tb[12];
  formatTime12h_buf(currentEpoch(), tb, sizeof(tb));
  display.setCursor(88, 24);
  display.print(tb);
}

// ---- dispatch ----// ---- dispatch ----

static void drawSecretMenuScreen() {
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("V: "); display.print(latestBatteryVoltage, 2); display.print("V");
  display.setCursor(0, 16);
  display.print("T: "); display.print((int)latestTemperatureC); display.print("C");
  display.setCursor(0, 32);
  display.print("Stor: 34%"); // TODO: real storage %
  display.display();
}

void drawOLED() {
  display.clearDisplay();

  extern bool secretMenuActive;
  if (secretMenuActive) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    drawSecretMenuScreen();
    return;
  }

  extern bool riderReadyActive;
  if (riderReadyActive) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    drawRiderReadyScreen();
    display.display();
    return;
  }

  extern volatile bool bleUploadActive;
  if (bleUploadActive) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    drawBleUploadScreen();
    display.display();
    return;
  }

  if (otaRequest) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    drawFwUpdateScreen();
    display.display();
    return;
  }

  if (wifiActive) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  bool monitorIdle = !otaRequest && !inPanic && !isCountingDown &&
                     !isArmingCountdown && !accState && !engineWasRunning &&
                     !isLogging;

  if (monitorIdle) {
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  display.ssd1306_command(SSD1306_DISPLAYON);

  if (otaRequest)             drawFwUpdateScreen();
  else if (inPanic)           drawPanicScreen();
  else if (isCountingDown)    drawCountdownScreen();
  else if (isArmingCountdown) drawArmingScreen();
  else                        drawRunningScreen();
  display.display();
}

void updateOLED_EdgeTriggered() {
  char key[48];

  extern volatile bool bleUploadActive;
  extern bool riderReadyActive;
  extern unsigned long riderReadyStart;
  if (riderReadyActive) {
    snprintf(key, sizeof(key), "RIDER|%lu",
             (millis() - riderReadyStart) / 1000UL);
  }
  else if (bleUploadActive) {
    snprintf(key, sizeof(key), "BLEUP");
  }
  else if (otaRequest) {
    snprintf(key, sizeof(key), "OTA|%d|%d",
             (int)otaStage,
             (int)otaRebootCountdown);
  } else if (wifiActive) {
    snprintf(key, sizeof(key), "WIFI|%s", wifiMessage);
  } else if (inPanic) {
    snprintf(key, sizeof(key), "PANIC|%d", (int)latestBatteryVoltage);
  } else if (isCountingDown) {
    snprintf(key, sizeof(key), "CD|%d",
             (int)((millis() - countdownStartMillis) / 1000));
  } else if (isArmingCountdown) {
    snprintf(key, sizeof(key), "ARM|%d",
             (int)((millis() - armingStartMillis) / 1000));
  } else if (accState || engineWasRunning || isLogging) {
    // V4.58: include GPS sats and fix in the key so the OLED redraws
    // when the satellite count changes.
    snprintf(key, sizeof(key), "RUN|%d|%d|%d|%d|%d",
             (int)latestBatteryVoltage, isLogging ? 1 : 0,
             isLogging ? (int)((millis() / 500) % 2) : 0,
             (int)gpsSats(), gpsHasFix() ? 1 : 0);
  } else {
    snprintf(key, sizeof(key), "MON");
  }

  if (strcmp(key, lastOLEDState) != 0) {
    strncpy(lastOLEDState, key, sizeof(lastOLEDState) - 1);
    lastOLEDState[sizeof(lastOLEDState) - 1] = 0;
    drawOLED();
  }
}