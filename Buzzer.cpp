#include "Buzzer.h"
#include "Config.h"

bool alarmSequenceActive = false;
int alarmStep = 0;
unsigned long alarmStepTimer = 0;
bool lowBattBeepActive = false;
int lowBattBeepStep = 0;
int lowBattBeepRemaining = 0;
unsigned long lowBattBeepTimer = 0;

void buzzerOn()  { ledcWriteTone(0, 2000); digitalWrite(STATUS_LED_PIN, HIGH); }
void buzzerOff() { ledcWrite(0, 0); digitalWrite(STATUS_LED_PIN, LOW); }

void updateBeeps(unsigned long now) {
  if (alarmSequenceActive) {
    if (alarmStep == 0) { buzzerOn(); alarmStepTimer = now; alarmStep = 1; }
    else if (alarmStep == 1 && now - alarmStepTimer >= 150) { buzzerOff(); alarmStepTimer = now; alarmStep = 2; }
    else if (alarmStep == 2 && now - alarmStepTimer >= 250) { buzzerOn(); alarmStepTimer = now; alarmStep = 3; }
    else if (alarmStep == 3 && now - alarmStepTimer >= 150) { buzzerOff(); alarmSequenceActive = false; }
  }
  if (lowBattBeepActive) {
    if (lowBattBeepStep == 0) { buzzerOn(); lowBattBeepTimer = now; lowBattBeepStep = 1; }
    else if (lowBattBeepStep == 1 && now - lowBattBeepTimer >= 150) { buzzerOff(); lowBattBeepTimer = now; lowBattBeepStep = 2; }
    else if (lowBattBeepStep == 2 && now - lowBattBeepTimer >= 150) {
      lowBattBeepRemaining--;
      if (lowBattBeepRemaining > 0) lowBattBeepStep = 0;
      else lowBattBeepActive = false;
    }
  }
}