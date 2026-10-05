#include "SerialBuffer.h"

static String buf[SERIAL_BUF_LINES];
static int idx = 0;

void serialBufInit() {
  for (int i = 0; i < SERIAL_BUF_LINES; i++) buf[i] = "";
  idx = 0;
}

void serialBufWrite(const char* s) {
  if (!s) return;
  buf[idx] = String(s);
  idx = (idx + 1) % SERIAL_BUF_LINES;
}

String serialBufGet() {
  String out;
  out.reserve(SERIAL_BUF_LINES * 64);
  for (int i = 0; i < SERIAL_BUF_LINES; i++) {
    int k = (idx + i) % SERIAL_BUF_LINES;
    if (buf[k].length() > 0) out += buf[k];
  }
  return out;
}

