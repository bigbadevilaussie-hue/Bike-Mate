#pragma once
#include <Arduino.h>

void serverSetup();
void serverLoop();
void serverStop();
bool serverIsRunning();

// Set to true when /maint/off is hit so the main loop can shut down
extern volatile bool maintOffRequested;

extern volatile bool uploadNowRequested;
