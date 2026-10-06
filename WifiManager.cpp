// WifiManager.cpp — BIKE-MATE V3.60
// WiFi bring-up using the proven IDF sequence from the diagnostic.
// V3.60: force SSID/password from Config.h (override stale NVS cache).
// V3.34: IDF teardown + re-init, 20 dBm, BSSID clear, PMF optional.
// V3.32: NTP failure fails bring-up.

#include "WifiManager.h"
#include "BleManager.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <esp_wifi.h>
#include <time.h>

extern void tprint(const char* fmt, ...);
extern uint32_t macTimeEpoch;
extern uint32_t secondsAtSync;
extern uint32_t totalSeconds;

char wifiMessage[24] = "";

static volatile uint32_t wifiLastEvent = 0;
static volatile uint32_t wifiLastDisconnectReason = 0;
static volatile int8_t wifiLastDisconnectRssi = 0;
static volatile uint32_t wifiEventCount = 0;

static void wifiDebugEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiLastEvent = (uint32_t)event;
  wifiEventCount++;

  switch (event) {
    case ARDUINO_EVENT_WIFI_READY:
      tprint("[WIFI-EVENT] READY");
      break;

    case ARDUINO_EVENT_WIFI_STA_START:
      tprint("[WIFI-EVENT] STA_START");
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
      char bssid[18];
      snprintf(bssid, sizeof(bssid),
               "%02X:%02X:%02X:%02X:%02X:%02X",
               info.wifi_sta_connected.bssid[0],
               info.wifi_sta_connected.bssid[1],
               info.wifi_sta_connected.bssid[2],
               info.wifi_sta_connected.bssid[3],
               info.wifi_sta_connected.bssid[4],
               info.wifi_sta_connected.bssid[5]);

      tprint("[WIFI-EVENT] STA_CONNECTED bssid=%s channel=%u",
             bssid,
             (unsigned)info.wifi_sta_connected.channel);
      break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifiLastDisconnectReason =
          (uint32_t)info.wifi_sta_disconnected.reason;
      wifiLastDisconnectRssi =
          info.wifi_sta_disconnected.rssi;

      tprint("[WIFI-EVENT] STA_DISCONNECTED reason=%u rssi=%d",
             (unsigned)info.wifi_sta_disconnected.reason,
             (int)info.wifi_sta_disconnected.rssi);

      tprint("[WIFI-EVENT] disconnect ssid='%.*s'",
             info.wifi_sta_disconnected.ssid_len,
             info.wifi_sta_disconnected.ssid);

      tprint("[WIFI-EVENT] bssid=%02X:%02X:%02X:%02X:%02X:%02X",
             info.wifi_sta_disconnected.bssid[0],
             info.wifi_sta_disconnected.bssid[1],
             info.wifi_sta_disconnected.bssid[2],
             info.wifi_sta_disconnected.bssid[3],
             info.wifi_sta_disconnected.bssid[4],
             info.wifi_sta_disconnected.bssid[5]);
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      tprint("[WIFI-EVENT] GOT_IP ip=%s mask=%s gw=%s",
             WiFi.localIP().toString().c_str(),
             WiFi.subnetMask().toString().c_str(),
             WiFi.gatewayIP().toString().c_str());
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      tprint("[WIFI-EVENT] LOST_IP");
      break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
      tprint("[WIFI-EVENT] STA_STOP");
      break;

    default:
      tprint("[WIFI-EVENT] event=%d", (int)event);
      break;
  }
}

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

// V4.92: fetch the router's Date header over LAN. The Opal
// (GL-SFT1200) is always up — even when the phone uplink is gone — so
// its clock is the fastest and most reliable source on the network.
// LAN RTT is ~5 ms, vs 60-100 ms for internet NTP. Falls back to NTP
// if this fails.
// V5.00: timegm() is not exposed by the ESP32 newlib headers.
// Convert a UTC struct tm to a Unix epoch manually. Standard
// days-from-civil algorithm (Howard Hinnant).
static time_t _utcToEpoch(int year, int mon, int day,
                          int hour, int min, int sec) {
  int y = year;
  unsigned m = (unsigned)mon;
  unsigned d = (unsigned)day;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const long long days = (long long)era * 146097LL + (long long)doe - 719468LL;
  return (time_t)(days * 86400LL + hour * 3600LL + min * 60LL + sec);
}

static bool opalClockSync() {
  IPAddress gw = WiFi.gatewayIP();
  if ((uint32_t)gw == 0) {
    tprint("[CLOCK] no gateway");
    return false;
  }
  char url[48];
  snprintf(url, sizeof(url), "http://%u.%u.%u.%u/",
           (unsigned)gw[0], (unsigned)gw[1],
           (unsigned)gw[2], (unsigned)gw[3]);
  tprint("[CLOCK] querying %s", url);

  HTTPClient http;
  http.setTimeout(3000);
  http.setReuse(false);
  if (!http.begin(url)) {
    tprint("[CLOCK] begin failed");
    return false;
  }
  const char* hdrs[] = { "Date" };
  http.collectHeaders(hdrs, 1);

  int code = http.GET();
  tprint("[CLOCK] HTTP code=%d", code);
  bool ok = false;
  if (code > 0) {
    String d = http.header("Date");
    if (d.length() > 0) {
      // "Tue, 06 Oct 2026 07:33:19 GMT"
      struct tm tmv = {};
      if (strptime(d.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tmv)) {
        time_t t = _utcToEpoch(
            tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
            tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        if (t > 1700000000) {
          macTimeEpoch = (uint32_t)t;
          secondsAtSync = totalSeconds;
          tprint("[CLOCK] Opal Date=%s -> %lu",
                 d.c_str(), (unsigned long)t);
          setMsg("Clock OK");
          ok = true;
        } else {
          tprint("[CLOCK] Opal time out of range: %lu", (unsigned long)t);
        }
      } else {
        tprint("[CLOCK] Date parse failed: %s", d.c_str());
      }
    } else {
      tprint("[CLOCK] no Date header (code=%d)", code);
    }
  } else {
    tprint("[CLOCK] HTTP GET failed: %d", code);
  }
  http.end();
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

static bool _wifiBringUpOnce(unsigned long perAttemptTimeoutMs) {
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

  // Static IP: reserved on Opal for Bike-Mate MAC AC:27:6E:26:0B:90
  IPAddress staticIP(192, 168, 8, 196);
  IPAddress gateway(192, 168, 8, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 8, 1);
  WiFi.config(staticIP, gateway, subnet, dns);

  tprint("[WIFI] static IP %s gw=%s",
         staticIP.toString().c_str(),
         gateway.toString().c_str());

  setMsg("Connecting");
  tprint("[WIFI] connecting to '%s'", WIFI_SSID);
  esp_err_t connErr = esp_wifi_connect();
  tprint("[WIFI] esp_wifi_connect = 0x%08X", connErr);

  unsigned long t0 = millis();
  unsigned long lastIpDiag = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastIpDiag >= 2000UL) {
      lastIpDiag = millis();
      tprint("[WIFI-IP] status=%d ip=%s mask=%s gw=%s",
             WiFi.status(),
             WiFi.localIP().toString().c_str(),
             WiFi.subnetMask().toString().c_str(),
             WiFi.gatewayIP().toString().c_str());
    }

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

  // V4.92: try the LAN router clock first. Opal is always up, even
  // with no upstream, and its Date header is ~20x faster than NTP.
  if (opalClockSync()) {
    tprint("[WIFI] clock from LAN");
  } else if (ntpSync()) {
    tprint("[WIFI] clock from NTP");
  } else {
    tprint("[WIFI] no clock source, continuing with stale clock");
    setMsg("Clock stale");
  }

  return true;
}

// V4.91: retry wrapper. On the C3 (core 2.0.17, bad antenna) WiFi
// bring-up after an active BLE session fails intermittently — the
// association never progresses and status stays WL_IDLE_STATUS. A
// second attempt after a full teardown + settle usually succeeds.
// Two attempts total, then give up (bike sleeps, retries next wake).
bool wifiBringUp(unsigned long perAttemptTimeoutMs) {
  WiFi.onEvent(wifiDebugEvent);
  for (int attempt = 1; attempt <= 2; attempt++) {
    tprint("[WIFI] attempt %d/2", attempt);
    if (_wifiBringUpOnce(perAttemptTimeoutMs)) {
      return true;
    }
    if (attempt < 2) {
      tprint("[WIFI] attempt %d failed, settling 3s before retry", attempt);
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(3000);
    }
  }
  tprint("[WIFI] all attempts failed");
  return false;
}

void wifiBringDown() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setMsg("");
  tprint("[WIFI] down");
  bleStart();
}