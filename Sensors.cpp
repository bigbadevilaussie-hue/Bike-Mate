// Sensors.cpp
// Reads voltage (GPIO 0) and temperature (GPIO 3) from ADC.

#include "Sensors.h"
#include "Config.h"
#include <math.h>

extern void tprint(const char* fmt, ...);

float latestBatteryVoltage = 12.6;
float latestTemperatureC = 20.0;
int latestRawVoltage = 0;
int latestMilliVolts = 0;
int latestNtcRaw = 0;

#define MEDIAN_SAMPLES 21
#define BATTERY_SLOPE  0.008058f

#define NTC_NOMINAL    10000.0f
#define NTC_B          3950.0f
#define NTC_SERIES     10000.0f

static int medianRawVoltage() {
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

static float readNTC() {
  int raw = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    raw += analogRead(THERMISTOR_PIN);
  }
  raw /= ADC_SAMPLES;
  latestNtcRaw = raw;

  float vOut = raw * (3.3f / 4095.0f);
  if (vOut <= 0.01f || vOut >= 3.29f) return -99.0f;

  // Correct pull-up formula (Fixed 10k resistor to 3.3V, Thermistor to GND)
  float rNtc = NTC_SERIES * vOut / (3.3f - vOut);

  float tK = 1.0f / (1.0f / (25.0f + 273.15f) +
                     log(rNtc / NTC_NOMINAL) / NTC_B);
  return tK - 273.15f;
}

void readSensors() {
  int raw = medianRawVoltage();
  latestRawVoltage = raw;
  latestMilliVolts = (int)(raw * 0.728f);
  latestBatteryVoltage = raw * BATTERY_SLOPE;

  latestTemperatureC = readNTC();
}