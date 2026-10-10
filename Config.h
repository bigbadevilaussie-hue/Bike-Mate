#pragma once
#if __has_include("Config.local.h")
#include "Config.local.h"
#else
#define WIFI_SSID        "YOUR_SSID"
#define WIFI_PASSWORD    "YOUR_PASSWORD"
#define GMAIL_SENDER     "your-email@gmail.com"
#define GMAIL_APP_PW     "your-app-password"
#define MAIL_RECIPIENT   "your-email@example.com"
#endif

// BIKE-MATE — configuration
// V4.70: dead defines removed, section headers added.
// Deploy-specific values (UPLOAD_URL, SMTP_*) still live here pending move to Deploy.h.

#define BIKE_MATE_VERSION "5.24"

// ---- Debug verbosity ----
#define DEBUG_VERBOSE 1

// ---- Display ----
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C

// ---- Pins ----
#define SDA_PIN 6
#define SCL_PIN 9
#define VOLTAGE_PIN 0
#define THERMISTOR_PIN 3
#define ACC_LED_PIN 1
#define BUZZER_PIN 4
#define STATUS_LED_PIN 10

// ---- GPS UART ----
#define GPS_RX_PIN 2
#define GPS_TX_PIN 21

// ---- BLE ----
#define DEVICE_NAME "Bike-Mate-2"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DATA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define STREAM_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define REQUEST_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ad"
#define OTA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ae"
#define SETTINGS_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b0"

// ---- Logging ----
// NOTE: LOG_INTERVAL_SEC is aspirational — actual ride cadence is TICK_MS.
// See audit #17. Decision pending: 5s or 30s?
#define LOG_INTERVAL_SEC 5

// ---- NVS namespaces ----
// Only UPLOAD is currently referenced. Others reserved for future use.
#define NVS_NAMESPACE_UPLOAD "upload"

// ---- Wake intervals ----
#define BENCH_MODE 1
#define WAKE_BENCH_MS    30000UL
#define WAKE_DAY_MS     300000UL
#define WAKE_NIGHT_MS   600000UL
#define NIGHT_START_HOUR   17
#define NIGHT_END_HOUR      8
#define MAX_AWAKE_MS    180000UL   // V4.72: 180 s hard max-awake, OTA exempt
#define BLE_MAX_CONN_MS  600000UL   // V4.73: force disconnect after 10 min
#define BLE_IDLE_GRACE_MS   5000UL   // V4.84: idle BLE conn does not block sleep
#define MAINT_MAX_MS     300000UL   // V4.75: 5 min maintenance cap (was 15)

// ---- Structs ----
struct RideRow {
  uint32_t epoch;
  int32_t  lat_x1e7;
  int32_t  lon_x1e7;
  uint16_t volt_x100;
  int8_t   temp;
  uint8_t  state;
};

struct RideSummary {
  uint32_t startEpoch;
  uint32_t endEpoch;
  uint32_t durationSecs;
  uint16_t minVolt_x100;
  uint16_t maxVolt_x100;
  uint16_t avgVolt_x100;
  int8_t   minTemp;
  int8_t   maxTemp;
  uint16_t underVoltSecs;
  uint16_t overVoltSecs;
  uint8_t  flags;
  uint8_t  rowCount_hi;
  uint16_t preRideVolt_x100;
  int32_t  startLat_x1e7;
  int32_t  startLon_x1e7;
  int32_t  endLat_x1e7;
  int32_t  endLon_x1e7;
  uint16_t rowCount;
  uint16_t pad;
};

struct WakeSample {
  uint32_t epoch;
  int32_t  lat_x1e7;
  int32_t  lon_x1e7;
  uint16_t volt_x100;
  int8_t   temp;
  uint8_t  state;
  uint8_t  flags;
  uint8_t  sats;
  uint8_t  pad;
};

// ---- Flags ----
#define FLAG_UNDER_VOLT 0x01
#define FLAG_OVER_VOLT 0x02
#define FLAG_FREEZING 0x04
#define FLAG_HOT_AMBIENT 0x08
#define FLAG_ENCLOSURE_HOT 0x10
#define FLAG_STORAGE_LOW 0x20   // V4.57: repurposed from FLAG_THERMAL_CUT
#define FLAG_PANIC 0x40
#define FLAG_GPS_FIX 0x80

#define TH_OVER_VOLT 14.8
#define TH_FREEZING 0.0
#define TH_HOT_AMBIENT 40.0
#define TH_ENCLOSURE_HOT 60.0

// ---- Timing ----
#define TICK_MS 30000UL
#define ADV_RESTART_MS 3000
#define MONITOR_ADV_MS 3000
#define SPLASH_MS 3000

// ---- Voltage thresholds ----
// V5.02: all voltage thresholds are now runtime settings in the
// bikeset NVS namespace. See Settings.h for the schema.
//   RUN MODE:     runningEnter, runningExit, runUnder, runOver
//   MONITOR MODE: monitorNormal, monitorWarning, monitorPanic
// V_RESTING_MAX remains a compile-time constant (tracking window
// ceiling — no user need to tune).
#define V_RESTING_MAX 13.2

// ---- ADC ----
// LIVE: ADC_SAMPLES is used in Sensors.cpp NTC averaging.
// DEAD (removed V4.70): R_TOP, R_BOTTOM, DIVIDER_RATIO, ADC_SLOPE,
// ADC_INTERCEPT, ADC_JUMP_LIMIT, V_PLAUSIBLE_MIN, V_PLAUSIBLE_MAX.
// The live battery calibration is BATTERY_SLOPE in Sensors.cpp.
#define ADC_SAMPLES 5

// ---- Unified WiFi bring-up ----
#define WIFI_ATTEMPT_TIMEOUT_MS 30000UL

// ---- Mail module ----
#define MAIL_ENABLED 1   // V4.71: enabled; bench gate is in sendMail()



#define SMTP_HOST        "smtp.gmail.com"
#define SMTP_PORT        465

// V5.02: WARN_EMAIL_VOLTAGE / WARN_RECOVER_VOLTAGE now settings.
// Recovery is derived: warning + 200 mV.
#define WARN_RECOVER_GAP_MV     200
#define LOW_VOLT_LOOPS_REQUIRED 2
#define MAIL_RATE_LIMIT_SEC     3600
#define MAIL_FAIL_BACKOFF       3
#define MAIL_FALLBACK_DAYS      7
// NOTE: audit #23 — _mailFailCount has no decay; backoff self-locks after 3 fails.
// TODO #9. (MAIL_FALLBACK_DAYS is the attempted mitigation — see WifiMail.cpp:96.)

// ---- OTA module ----
#define OTA_ENABLED       1
#define OTA_TIMEOUT_MS    1800000UL
#define OTA_URL_MAX       192
#define OTA_MD5_LEN       33

// ---- Drive upload module ----
#define UPLOAD_ENABLED 1
#define UPLOAD_HOUR_LOCAL 4
#define UPLOAD_URL "https://script.google.com/macros/s/AKfycbx758YfZhY4wp_FdWgu6DSoDVn6-k0Wb0hVVDzwauRSZOSbT9zFAsvskeHv4mz5-G59/exec"
#define UPLOAD_URL_MAX 256
#define UPLOAD_WIFI_TIMEOUT_MS 30000UL
#define UPLOAD_HTTP_TIMEOUT_MS 60000UL
#define UPLOAD_COOLDOWN_MS 60000UL
