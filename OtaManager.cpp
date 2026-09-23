// OtaManager.cpp
// Uses unified WifiManager for radio bring-up.
// BLE is stopped before WiFi, restarted after.
// Progress tracked for OLED. 5s countdown before reboot. Version from GUI.

#include "OtaManager.h"
#include "Config.h"
#include "WifiManager.h"
#include "DisplayManager.h"

#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

extern void tprint(const char* fmt, ...);

// ---- RTC-persistent OTA state ----
RTC_DATA_ATTR volatile bool otaRequest = false;
RTC_DATA_ATTR char otaUrl[OTA_URL_MAX] = "";
RTC_DATA_ATTR uint32_t otaSize = 0;
RTC_DATA_ATTR char otaMd5[OTA_MD5_LEN] = "";
RTC_DATA_ATTR char otaVersion[16] = "";
RTC_DATA_ATTR unsigned long otaStartMillis = 0;

// ---- progress tracking ----
volatile uint32_t otaProgressBytes = 0;
volatile uint32_t otaProgressTotal = 0;
volatile uint8_t  otaStage = OTA_STAGE_IDLE;
volatile uint8_t  otaRebootCountdown = 0;

bool otaInProgress() {
  return otaRequest;
}

// ---- Main OTA routine ----
bool otaPerformUpdate(const char* url, uint32_t expectedSize, const char* expectedMd5) {
  tprint("[OTA] starting update");
  tprint("[OTA] url=%s", url);
  tprint("[OTA] size=%lu", (unsigned long)expectedSize);
  tprint("[OTA] md5=%s", expectedMd5);

  otaProgressBytes = 0;
  otaProgressTotal = 0;
  otaStage = OTA_STAGE_IDLE;

  if (!wifiBringUp()) {
    tprint("[OTA] WiFi bring-up failed");
    wifiBringDown();
    return false;
  }

  HTTPClient http;
  http.setTimeout(30000);
  http.begin(url);
  int code = http.GET();
  if (code != 200) {
    tprint("[OTA] HTTP GET failed: %d", code);
    http.end();
    wifiBringDown();
    return false;
  }

  int contentLen = http.getSize();
  tprint("[OTA] content-length: %d", contentLen);
  if (contentLen <= 0) {
    tprint("[OTA] invalid content-length");
    http.end();
    wifiBringDown();
    return false;
  }
  if (expectedSize > 0 && (uint32_t)contentLen != expectedSize) {
    tprint("[OTA] size mismatch: expected %lu got %d",
           (unsigned long)expectedSize, contentLen);
    http.end();
    wifiBringDown();
    return false;
  }

  if (!Update.begin(contentLen)) {
    tprint("[OTA] Update.begin failed: %s", Update.errorString());
    http.end();
    wifiBringDown();
    return false;
  }

  tprint("[OTA] DOWNLOADING %d bytes", contentLen);

  otaProgressTotal = (uint32_t)contentLen;
  otaStage = OTA_STAGE_DOWNLOAD;

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  uint32_t written = 0;
  unsigned long lastProgress = 0;

  while (http.connected() && written < (uint32_t)contentLen) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(buf, min((size_t)sizeof(buf), avail));
      if (n <= 0) break;
      size_t w = Update.write(buf, n);
      if (w != (size_t)n) {
        tprint("[OTA] Update.write short: %u/%d", (unsigned)w, n);
        break;
      }
      written += w;
      otaProgressBytes = written;
      if (millis() - lastProgress > 500) {
        lastProgress = millis();
        tprint("[OTA] %lu / %d", (unsigned long)written, contentLen);
        drawOLED();
      }
    } else {
      delay(10);
    }
  }

  http.end();

  if (written != (uint32_t)contentLen) {
    tprint("[OTA] INCOMPLETE: %lu / %d", (unsigned long)written, contentLen);
    Update.abort();
    wifiBringDown();
    return false;
  }

  tprint("[OTA] download complete");

  otaStage = OTA_STAGE_VERIFY;

  if (!Update.end(true)) {
    tprint("[OTA] Update.end failed: %s", Update.errorString());
    wifiBringDown();
    return false;
  }

  otaStage = OTA_STAGE_FLASH;

  tprint("[OTA] FILE INSTALLED OK");

  String actualMd5 = Update.md5String();
  if (expectedMd5 && strlen(expectedMd5) > 0) {
    if (!actualMd5.equalsIgnoreCase(expectedMd5)) {
      tprint("[OTA] MD5 MISMATCH: expected %s got %s",
             expectedMd5, actualMd5.c_str());
      wifiBringDown();
      return false;
    }
    tprint("[OTA] MD5 VERIFIED");
  }

  tprint("[OTA] NVS FLAG SET");

  Preferences prefs;
  prefs.begin("ota", false);
  prefs.putBool("pending", true);
  prefs.end();

  wifiBringDown();

  tprint("[OTA] ====== REBOOTING NOW ======");

  otaStage = OTA_STAGE_REBOOT;
  for (int i = 5; i > 0; i--) {
    otaRebootCountdown = (uint8_t)i;
    tprint("[OTA] reboot in %d...", i);
    drawOLED();
    delay(1000);
  }

  display.clearDisplay();
  display.display();
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  ESP.restart();

  return true;
}