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
// V3.64: BENCH_MODE 1, UPLOAD_ENABLED 1, new Apps Script deployment

#define BIKE_MATE_VERSION "4.16"
#define BENCH_MODE 1

// ---- Debug verbosity ----
#define DEBUG_VERBOSE 0

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

// ---- GPS stub ----
#define GPS_STUB_ENABLED 1
#define GPS_STUB_LAT    (-27.420264910682718)
#define GPS_STUB_LON    ( 152.45246261005468)
#define GPS_STUB_SATS   8

// ---- BLE ----
#define DEVICE_NAME "Bike-Mate"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define DATA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TIME_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define STREAM_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define REQUEST_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ad"
#define OTA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ae"

// ---- Logging ----
#define LOG_INTERVAL_SEC 5
#define WAKE_LOG_INTERVAL_SEC 300
#define MAX_ROWS_PER_RIDE 1024

#define NVS_NAMESPACE_SYSTEM "system"
#define NVS_NAMESPACE_MAIL   "mail"
#define NVS_NAMESPACE_OTA    "ota"
#define NVS_NAMESPACE_UPLOAD "upload"

#define RIDE_FILE_PREFIX    "/ride_"

// ---- Wake intervals ----
#define WAKE_BENCH_MS    30000UL
#define WAKE_DAY_MS     300000UL
#define WAKE_NIGHT_MS   600000UL
#define NIGHT_START_HOUR   22
#define NIGHT_END_HOUR      4

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
#define FLAG_THERMAL_CUT 0x20
#define FLAG_PANIC 0x40
#define FLAG_GPS_FIX 0x80

#define TH_UNDER_RUN 13.8
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
#define V_RUNNING_ENTER 13.8
#define V_RUNNING_EXIT 13.0
#define V_PANIC_ENTER 12.0
#define V_PANIC_EXIT 12.4

#define V_RESTING_MIN 12.4
#define V_RESTING_MAX 13.2

// ---- ADC ----
#define R_TOP 100000.0
#define R_BOTTOM 10000.0
#define DIVIDER_RATIO 10.771f
#define ADC_SAMPLES 5
#define ADC_JUMP_LIMIT 3
#define V_PLAUSIBLE_MIN 8.0
#define V_PLAUSIBLE_MAX 20.0
#define ADC_SLOPE 0.00073643f
#define ADC_INTERCEPT (-0.01602f)

// ---- Unified WiFi bring-up ----
#define WIFI_ATTEMPT_TIMEOUT_MS 30000UL

// ---- Mail module ----
#define MAIL_ENABLED 1



#define SMTP_HOST        "smtp.gmail.com"
#define SMTP_PORT        465

#define WARN_EMAIL_VOLTAGE      12.50
#define WARN_RECOVER_VOLTAGE    12.70
#define LOW_VOLT_LOOPS_REQUIRED 2
#define MAIL_RATE_LIMIT_SEC     3600
#define MAIL_FALLBACK_DAYS      7
#define MAIL_FAIL_BACKOFF       3

// ---- OTA module ----
#define OTA_ENABLED       1
#define OTA_TIMEOUT_MS    1800000UL
#define OTA_URL_MAX       192
#define OTA_MD5_LEN       33

// ---- Drive upload module ----
#define UPLOAD_ENABLED 1
#define UPLOAD_HOUR_LOCAL 4
#define UPLOAD_URL "https://script.google.com/macros/s/AKfycbz3xknkHcub61nntHvPqYrTFEmhKqHn_S1GhoUya0kAzrf-N89lIP6JA9tmpzSJV6mL/exec"
#define UPLOAD_URL_MAX 256
#define UPLOAD_WIFI_TIMEOUT_MS 30000UL
#define UPLOAD_HTTP_TIMEOUT_MS 60000UL
#define UPLOAD_COOLDOWN_MS 60000UL
