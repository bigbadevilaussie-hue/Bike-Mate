#pragma once

#include <Arduino.h>

extern bool alarmSequenceActive;
extern int alarmStep;
extern unsigned long alarmStepTimer;
extern bool lowBattBeepActive;
extern int lowBattBeepStep;
extern int lowBattBeepRemaining;
extern unsigned long lowBattBeepTimer;

void buzzerOn();
void buzzerOff();
void updateBeeps(unsigned long now);