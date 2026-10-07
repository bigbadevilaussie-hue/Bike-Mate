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

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
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

void drawUploadScreen() {
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  const char* title = "Maintenance";
  int tw = strlen(title) * 6;
  display.setCursor((SCREEN_W - tw) / 2, 8);
  display.print(title);

  display.drawLine(20, 22, SCREEN_W - 20, 22, SSD1306_WHITE);

  display.setTextSize(2);
  const char* big = "Uploading";
  int bw = strlen(big) * 12;
  display.setCursor((SCREEN_W - bw) / 2, 34);
  display.print(big);

  display.display();
}

static void drawMaintScreen() {
  extern unsigned long maintStartMs;
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  const char* big = "MAINT";
  int bw = strlen(big) * 12;
  display.setCursor((SCREEN_W - bw) / 2, 4);
  display.print(big);

  display.setTextSize(1);
  const char* sub = "MODE";
  int sw = strlen(sub) * 6;
  display.setCursor((SCREEN_W - sw) / 2, 24);
  display.print(sub);

  // V4.89: show wifiMessage, not WiFi.localIP(). Including <WiFi.h>
  // here initialises the netif before wifiBringUp() can tear it down,
  // which broke association on the C3. wifiMessage is set by
  // WifiManager during bring-up.
  extern char wifiMessage[24];
  char msg[24];
  snprintf(msg, sizeof(msg), "%s", wifiMessage);
  int iw = strlen(msg) * 6;
  if (iw > SCREEN_W - 4) iw = SCREEN_W - 4;
  display.setCursor((SCREEN_W - iw) / 2, 36);
  display.print(msg);

  // remaining
  unsigned long now = millis();
  unsigned long elapsed = (now >= maintStartMs) ? (now - maintStartMs) : 0;
  unsigned long cap = MAINT_MAX_MS;
  long rem = (long)((cap - elapsed) / 1000UL);
  if (rem < 0) rem = 0;
  char rb[20];
  snprintf(rb, sizeof(rb), "%lds left", rem);
  int rw = strlen(rb) * 6;
  display.setCursor((SCREEN_W - rw) / 2, 52);
  display.print(rb);

  display.display();
}

static void drawFwUpdateScreen() {
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  const char* title = "Firmware Update";
  int tw = strlen(title) * 6;
  display.setCursor((SCREEN_W - tw) / 2, 8);
  display.print(title);

  char verBuf[24];
  if (otaVersion[0]) {
    snprintf(verBuf, sizeof(verBuf), "Ver %s", otaVersion);
  } else {
    snprintf(verBuf, sizeof(verBuf), "Ver --");
  }
  display.setTextSize(2);
  int vw = strlen(verBuf) * 12;
  display.setCursor((SCREEN_W - vw) / 2, 24);
  display.print(verBuf);

  display.setTextSize(1);
  char status[24];

  if (otaStage == OTA_STAGE_DOWNLOAD) {
    snprintf(status, sizeof(status), "Downloading...");
  } else if (otaStage == OTA_STAGE_VERIFY) {
    snprintf(status, sizeof(status), "Verifying...");
  } else if (otaStage == OTA_STAGE_FLASH) {
    snprintf(status, sizeof(status), "Flashing...");
  } else if (otaStage == OTA_STAGE_REBOOT) {
    snprintf(status, sizeof(status), "Rebooting in %ds", otaRebootCountdown);
  } else {
    snprintf(status, sizeof(status), "Starting...");
  }

  int sw = strlen(status) * 6;
  display.setCursor((SCREEN_W - sw) / 2, 52);
  display.print(status);

  display.display();
}

static void drawPanicScreen() {
  display.fillRect(0, 0, SCREEN_W, SCREEN_H, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(28, 8); display.print("LOW BATTERY");
  display.setTextSize(3);
  display.setCursor(16, 24);
  display.print(latestBatteryVoltage, 1);
  display.print("V");
  display.setTextSize(1);
  display.setCursor(40, 52); display.print("PANIC");
  display.display();
}

static void drawCountdownScreen() {
  int rem = countdownSeconds - ((millis() - countdownStartMillis) / 1000);
  if (rem < 0) rem = 0;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print("ENGINE START");
  display.setCursor(96, 0); display.print((int)latestTemperatureC); display.print("C");
  display.setTextSize(3);
  display.setCursor(44, 20); display.print(rem); display.print("s");
  display.setTextSize(1);
  display.setCursor(0, 52); display.print(accState ? "ACC ON" : "ACC OFF");
  display.display();
}

static void drawArmingScreen() {
  int rem = armingSeconds - ((millis() - armingStartMillis) / 1000);
  if (rem < 0) rem = 0;
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print("ENGINE STOP");
  display.setCursor(96, 0); display.print((int)latestTemperatureC); display.print("C");
  display.setTextSize(3);
  display.setCursor(44, 20); display.print(rem); display.print("s");
  display.setTextSize(1);
  display.setCursor(0, 52); display.print(latestBatteryVoltage, 1); display.print("V");
  display.display();
}

static void drawRunningScreen() {
  extern float gpsSpeed_kmh();
  char tb[12];
  formatTime12h_buf(currentEpoch(), tb, sizeof(tb));
  display.setTextColor(SSD1306_WHITE);

  // Top row: REC + GPS
  display.setTextSize(1);
  display.setCursor(8, 0);
  display.print("REC");
  drawRecDot(2, 4, isLogging);

  drawGpsBox(72, 1, gpsHasFix());
  display.setCursor(82, 0);
  display.print("GPS ");
  display.print(gpsSats());

  // Middle: big speed, no units
  char spd[8];
  if (gpsHasFix()) {
    int kph = (int)(gpsSpeed_kmh() + 0.5f);
    if (kph < 2) kph = 0;
    snprintf(spd, sizeof(spd), "%d", kph);
  } else {
    snprintf(spd, sizeof(spd), "--");
  }
  display.setTextSize(4);
  int slen = strlen(spd);
  int sx = (SCREEN_W - slen * 24) / 2;
  if (sx < 0) sx = 0;
  display.setCursor(sx, 18);
  display.print(spd);

  // Bottom row: volts, temp, time
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(latestBatteryVoltage, 1);
  display.print("V");

  display.setCursor(48, 56);
  display.print((int)latestTemperatureC);
  display.print("C");

  display.setCursor(88, 56);
  display.print(tb);

  display.display();
}

// ---- dispatch ----

void drawOLED() {
  display.clearDisplay();

  if (otaRequest) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    drawFwUpdateScreen();
    return;
  }

  if (wifiActive) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    if (serverIsRunning() && !otaRequest) {
      drawMaintScreen();
    } else {
      drawUploadScreen();
    }
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
}

void updateOLED_EdgeTriggered() {
  char key[48];

  if (otaRequest) {
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