// Sensors.cpp
// Reads voltage from real ADC via median filter on raw counts.

#include "Sensors.h"
#include "Config.h"
#include <math.h>

extern void tprint(const char* fmt, ...);

float latestBatteryVoltage = 12.6;
float latestTemperatureC = 20.0;
int latestRawVoltage = 0;
int latestMilliVolts = 0;

#define MEDIAN_SAMPLES 21
#define BATTERY_SLOPE 0.008058f

static int medianRaw() {
  int buf[MEDIAN_SAMPLES];
  for (int i = 0; i < MEDIAN_SAMPLES; i++) {
    buf[i] = analogRead(VOLTAGE_PIN);
    delayMicroseconds(200);
  }
  for (int i = 1; i < MEDIAN_SAMPLES; i++) {
    int key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key) {
      buf[j + 1] = buf[j];
      j--;
    }
    buf[j + 1] = key;
  }
  return buf[MEDIAN_SAMPLES / 2];
}

void readSensors() {
  int raw = medianRaw();
  latestRawVoltage = raw;
  latestMilliVolts = (int)(raw * 0.728f);
  latestBatteryVoltage = raw * BATTERY_SLOPE;
  latestTemperatureC = 25.0;
}
