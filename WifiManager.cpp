// WifiManager.cpp — BIKE-MATE V3.60
// WiFi bring-up using the proven IDF sequence from the diagnostic.
// V3.60: force SSID/password from Config.h (override stale NVS cache).
// V3.34: IDF teardown + re-init, 20 dBm, BSSID clear, PMF optional.
// V3.32: NTP failure fails bring-up.

#include "WifiManager.h"
#include "BleManager.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <time.h>

extern void tprint(const char* fmt, ...);
extern uint32_t macTimeEpoch;
extern uint32_t secondsAtSync;
extern uint32_t totalSeconds;

char wifiMessage[24] = "";

static void setMsg(const char* m) {
  snprintf(wifiMessage, sizeof(wifiMessage), "%s", m);
}

bool wifiPingTest() {
  WiFiClient test;
  test.setTimeout(5000);
  bool ok = test.connect("www.google.com", 443);
  test.stop();
  return ok;
}

static bool ntpSync() {
  setMsg("NTP sync");
  tprint("[WIFI] NTP sync");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  unsigned long t0 = millis();
  time_t now = 0;
  while (millis() - t0 < 20000UL) {
    time(&now);
    if (now > 1700000000) {
      macTimeEpoch = (uint32_t)now;
      secondsAtSync = totalSeconds;
      tprint("[WIFI] NTP OK %lu", (unsigned long)now);
      setMsg("NTP OK");
      return true;
    }
    delay(500);
  }
  tprint("[WIFI] NTP timeout");
  setMsg("NTP fail");
  return false;
}

bool wifiBringUp(unsigned long perAttemptTimeoutMs) {
  tprint("[WIFI] bring-up start");

  bleStop();
  delay(800);

  // V3.34: full IDF teardown + re-init (proven 72/72 on Opal)
  esp_wifi_stop();
  esp_wifi_deinit();
  delay(300);

  WiFi.mode(WIFI_STA);
  delay(100);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_start();
  delay(100);

  // TX power 8.5 dBm (raw 34) — proven on both boards
  esp_wifi_set_max_tx_power(34);

  // V3.60: force SSID + password, clear BSSID, PMF optional
  {
    wifi_config_t wc = {};
    esp_wifi_get_config(WIFI_IF_STA, &wc);

    memset(wc.sta.ssid, 0, sizeof(wc.sta.ssid));
    memset(wc.sta.password, 0, sizeof(wc.sta.password));
    strncpy((char*)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char*)wc.sta.password, WIFI_PASSWORD, sizeof(wc.sta.password) - 1);

    memset(wc.sta.bssid, 0, 6);
    wc.sta.bssid_set = 0;
    wc.sta.pmf_cfg.required = false;

    esp_wifi_set_config(WIFI_IF_STA, &wc);
  }

  setMsg("Connecting");
  tprint("[WIFI] connecting to '%s'", WIFI_SSID);
  esp_err_t connErr = esp_wifi_connect();
  tprint("[WIFI] esp_wifi_connect = 0x%08X", connErr);

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > perAttemptTimeoutMs) {
      tprint("[WIFI] timeout after %lu ms (status=%d)",
             (unsigned long)perAttemptTimeoutMs, WiFi.status());
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      setMsg("WiFi fail");
      return false;
    }
    delay(400);
  }

  tprint("[WIFI] OK %s", WiFi.localIP().toString().c_str());
  tprint("[WIFI] RSSI %d dBm", WiFi.RSSI());
  setMsg("Connected");

  if (!wifiPingTest()) {
    tprint("[WIFI] ping FAIL");
    setMsg("Ping fail");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
  tprint("[WIFI] ping OK");
  setMsg("Ping OK");

  if (!ntpSync()) {
    tprint("[WIFI] NTP failed");
    setMsg("NTP fail");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }

  return true;
}

void wifiBringDown() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setMsg("");
  tprint("[WIFI] down");
  bleStart();
}