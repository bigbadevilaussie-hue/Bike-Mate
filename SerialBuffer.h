#pragma once
#include <Arduino.h>

#define SERIAL_BUF_LINES 20

void   serialBufInit();
void   serialBufWrite(const char* s);
String serialBufGet();

