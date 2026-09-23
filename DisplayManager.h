#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// V3.00: standard Adafruit_SSD1306 (external 128x64 OLED)
extern Adafruit_SSD1306 display;
extern char lastOLEDState[48];

void drawOLED();
void updateOLED_EdgeTriggered();
void drawUploadScreen();
