#include "Buzzer.h"
#include "Config.h"

bool alarmSequenceActive = false;
int alarmStep = 0;
unsigned long alarmStepTimer = 0;
bool lowBattBeepActive = false;
int lowBattBeepStep = 0;
int lowBattBeepRemaining = 0;
unsigned long lowBattBeepTimer = 0;

// V5.49: locked circuit — bit-bang ~2.8 kHz, 90 ms chirps, 110 ms gap.
#define HALF_PERIOD_US 178   // ~2.8 kHz
#define CHIRP_MS        90
#define CHIRP_GAP_MS   110

static void chirpOnce(unsigned int ms) {
  digitalWrite(STATUS_LED_PIN, HIGH);
  unsigned long start = millis();
  while (millis() - start < ms) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(HALF_PERIOD_US);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(HALF_PERIOD_US);
  }
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(STATUS_LED_PIN, LOW);
}

void buzzerOn()  { chirpOnce(CHIRP_MS); }
void buzzerOff() { digitalWrite(BUZZER_PIN, LOW); digitalWrite(STATUS_LED_PIN, LOW); }

// Two chirps with a gap. Called once per alarm event.
void buzzerChirpPair() {
  chirpOnce(CHIRP_MS);
  delay(CHIRP_GAP_MS);
  chirpOnce(CHIRP_MS);
}

void updateBeeps(unsigned long now) {
  if (alarmSequenceActive) {
    // V5.49: one chirp pair per event. Locked cadence.
    buzzerChirpPair();
    alarmSequenceActive = false;
  }
  if (lowBattBeepActive) {
    // V5.49: one chirp pair per PANIC wake. Same sound as the alarm.
    buzzerChirpPair();
    lowBattBeepActive = false;
    lowBattBeepRemaining = 0;
  }
}