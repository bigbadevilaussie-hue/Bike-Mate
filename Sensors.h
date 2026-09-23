#pragma once

#include <Arduino.h>

extern float latestBatteryVoltage;
extern float latestTemperatureC;
extern int latestRawVoltage;
extern int latestMilliVolts;
extern int latestNtcRaw;

void readSensors();