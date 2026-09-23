#pragma once

#include <Arduino.h>

// Reads the battery voltage divider and the thermistor.
// Updates the shared latest* values, which are defined in Sensors.cpp.

extern float latestBatteryVoltage;
extern float latestTemperatureC;
extern int   latestRawVoltage;
extern int   latestMilliVolts;
extern int   latestRawThermistor;

float readTemperature(int &rawOut);
void  readSensors();