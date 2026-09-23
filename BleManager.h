#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE server, characteristics, callbacks.
// V3.11: bleStop() / bleStart() for WiFi coexistence.

extern BLEServer* pServer;
extern BLECharacteristic* pDataChar;
extern BLECharacteristic* pTimeChar;
extern BLECharacteristic* pStreamChar;
extern BLECharacteristic* pRequestChar;
extern BLECharacteristic* pOtaChar;

extern bool bleInited;
extern volatile bool pushRequested;
extern volatile bool pushInProgress;
extern volatile uint32_t pushGuiEpoch;

void bleInit();
void publishBLE();
void pushNewSlots(uint32_t guiHighestEpoch);
bool isActuallyConnected();

// V3.11: pause BLE for WiFi operations (single 2.4 GHz radio)
void bleStop();
void bleStart();