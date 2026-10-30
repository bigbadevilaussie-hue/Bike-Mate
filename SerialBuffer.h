#pragma once
#include <Arduino.h>

#define SERIAL_BUF_LINES 100

void   serialBufInit();
void   serialBufWrite(const char* s);
String serialBufGet();

