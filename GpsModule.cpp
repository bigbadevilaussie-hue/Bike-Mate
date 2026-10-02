#include "GpsModule.h"
#include "Config.h"

#include <time.h>

extern void tprint(const char* fmt, ...);

// GPS UART pins - on the C3 SuperMini, UART0 on GPIO 20/21 is free
// because the USB console uses USB-Serial-JTAG.
#ifndef GPS_RX_PIN
#define GPS_RX_PIN 2
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN 21
#endif
#define GPS_BAUD 9600

// Internal state
static char     _line[128];
static uint8_t  _lineLen = 0;

static bool     _hasFix = false;
RTC_DATA_ATTR static int32_t  _lat_x1e7 = 0;
RTC_DATA_ATTR static int32_t  _lon_x1e7 = 0;
RTC_DATA_ATTR static uint8_t  _sats = 0;
static float    _speed_kmh = 0.0f;
static float    _altitude_m = 0.0f;
static uint32_t _epochUTC = 0;
static uint32_t _lastSentenceMs = 0;

// Parse helper - split on commas, return field N as float (or NAN on empty)
static float _fieldFloat(char* buf, uint8_t n) {
  uint8_t idx = 0;
  char* p = buf;
  while (*p && idx < n) {
    if (*p == ',') idx++;
    p++;
  }
  if (idx < n || *p == 0) return NAN;
  if (*p == ',' || *p == '*') return NAN;
  return atof(p);
}

// Return pointer to field N (or NULL)
static char* _fieldStr(char* buf, uint8_t n) {
  uint8_t idx = 0;
  char* p = buf;
  while (*p && idx < n) {
    if (*p == ',') idx++;
    p++;
  }
  if (idx < n) return NULL;
  return p;
}

// Verify NMEA checksum. Line must include $...*HH
static bool _checksumOk(const char* line, uint8_t len) {
  if (len < 4) return false;
  if (line[0] != '$') return false;
  uint8_t sum = 0;
  for (uint8_t i = 1; i < len; i++) {
    if (line[i] == '*') break;
    sum ^= (uint8_t)line[i];
  }
  // Find the '*'
  uint8_t star = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (line[i] == '*') { star = i; break; }
  }
  if (star == 0 || star + 3 > len) return false;
  uint8_t expected = (uint8_t)strtol(&line[star + 1], NULL, 16);
  return sum == expected;
}

// Convert NMEA ddmm.mmmm + hemisphere to x1e7 integer degrees
static int32_t _nmeaTo_x1e7(const char* field, char hemi) {
  if (!field || *field == 0) return 0;
  float val = atof(field);
  if (val == 0.0f) return 0;
  int deg = (int)(val / 100.0f);
  float minutes = val - (deg * 100.0f);
  float dec = deg + minutes / 60.0f;
  if (hemi == 'S' || hemi == 'W') dec = -dec;
  return (int32_t)(dec * 1e7);
}

// Parse a complete NMEA sentence
static void _parseLine(char* line, uint8_t len) {
  if (!_checksumOk(line, len)) return;
  _lastSentenceMs = millis();

  // GPRMC - recommended minimum
  if (strncmp(line, "$GPRMC,", 7) == 0 || strncmp(line, "$GNRMC,", 7) == 0) {
    char* status = _fieldStr(line, 2);
    if (status && *status == 'A') {
      _hasFix = true;
      char* latF = _fieldStr(line, 3);
      char* latH = _fieldStr(line, 4);
      char* lonF = _fieldStr(line, 5);
      char* lonH = _fieldStr(line, 6);
      char* spdF = _fieldStr(line, 7);
      if (latF && latH) _lat_x1e7 = _nmeaTo_x1e7(latF, *latH);
      if (lonF && lonH) _lon_x1e7 = _nmeaTo_x1e7(lonF, *lonH);
      if (spdF && *spdF) {
        float knots = atof(spdF);
        _speed_kmh = knots * 1.852f;
      }
    } else {
      _hasFix = false;
    }
  }
  // GPGGA - fix quality, sats, altitude
  else if (strncmp(line, "$GPGGA,", 7) == 0 || strncmp(line, "$GNGGA,", 7) == 0) {
    char* fixQ = _fieldStr(line, 6);
    char* satsF = _fieldStr(line, 7);
    char* altF = _fieldStr(line, 9);
    if (fixQ) {
      int q = atoi(fixQ);
      _hasFix = (q > 0);
    }
    if (satsF && *satsF) _sats = (uint8_t)atoi(satsF);
    if (altF && *altF) _altitude_m = atof(altF);
  }
  // GPVTG - track + speed
  else if (strncmp(line, "$GPVTG,", 7) == 0 || strncmp(line, "$GNVTG,", 7) == 0) {
    char* kmhF = _fieldStr(line, 7);
    if (kmhF && *kmhF) {
      float kmh = atof(kmhF);
      if (kmh > 0.0f) _speed_kmh = kmh;
    }
  }
  // GPGSV - fallback satellite count if GPGGA has none
  else if (strncmp(line, "$GPGSV,", 7) == 0 || strncmp(line, "$GNGSV,", 7) == 0) {
    // Field 3 = total sats in view
    char* satsF = _fieldStr(line, 3);
    if (satsF && *satsF) {
      uint8_t s = (uint8_t)atoi(satsF);
      if (s > _sats) _sats = s;
    }
  }
}

void gpsModuleInit() {
  Serial1.setRxBufferSize(256);
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(300);
  int avail = Serial1.available();
  tprint("[GPS] UART init RX=%d TX=%d baud=%d avail=%d",
         GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD, avail);
}

void gpsModuleTick() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\n') {
      _line[_lineLen] = 0;
      if (_lineLen > 6 && _line[0] == '$') {
        _parseLine(_line, _lineLen);
      }
      _lineLen = 0;
    } else if (c != '\r' && _lineLen < sizeof(_line) - 1) {
      _line[_lineLen++] = c;
    } else if (_lineLen >= sizeof(_line) - 1) {
      _lineLen = 0;   // overflow, discard
    }
  }
}

bool gpsHasFix() {
#if GPS_STUB_ENABLED
  return true;
#else
  return _hasFix;
#endif
}

int32_t gpsLat_x1e7() {
#if GPS_STUB_ENABLED
  return (int32_t)(GPS_STUB_LAT * 1e7);
#else
  return _lat_x1e7;
#endif
}

int32_t gpsLon_x1e7() {
#if GPS_STUB_ENABLED
  return (int32_t)(GPS_STUB_LON * 1e7);
#else
  return _lon_x1e7;
#endif
}

uint8_t gpsSats() {
#if GPS_STUB_ENABLED
  return GPS_STUB_SATS;
#else
  return _sats;
#endif
}

float gpsSpeed_kmh() {
#if GPS_STUB_ENABLED
  return 0.0f;
#else
  return _speed_kmh;
#endif
}

float gpsAltitude_m() {
#if GPS_STUB_ENABLED
  return 0.0f;
#else
  return _altitude_m;
#endif
}

uint32_t gpsEpochUTC() {
#if GPS_STUB_ENABLED
  return 0;
#else
  return _epochUTC;
#endif
}
