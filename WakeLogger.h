#pragma once

#include <Arduino.h>
#include "Config.h"

// WakeLogger — writes one sample per wake to /wakes_YYYY-MM-DD.csv
//
// V3.17:
//   - Added wakeLoggerForceRotate() for bench mode
// V3.02:
//   - Date-based filenames
//   - Pause/resume for upload and OTA
//   - Upload results logged as "UPLOAD,..." lines

bool wakeLoggerInit();
void wakeLoggerTick();
void wakeLoggerForceWrite();

// Pause/resume — no writes while paused. Used by DriveUpload and OTA.
void wakeLoggerPause();
void wakeLoggerResume();
bool wakeLoggerIsPaused();

// Seal the current file and open a new one for the given trigger epoch.
// Called at 04:00 upload trigger. Returns true if new file opened.
bool wakeLoggerRotate(uint32_t triggerEpoch);

// V3.17: Force rotate — seals the current file with a .sealed suffix
// and opens a fresh file for the same day. Bench-mode only.
bool wakeLoggerForceRotate(uint32_t triggerEpoch);

// Log upload result lines to current wake file.
void wakeLoggerLogUpload(const char* filename, bool ok, const char* detail);
void wakeLoggerLogUploadStart();
void wakeLoggerLogUploadDone(int okCount, int failCount);

// File ops
size_t wakeLoggerFileSize();
bool   wakeLoggerTruncate();

// GPS location for next sample
void wakeLoggerSetLocation(int32_t lat_x1e7, int32_t lon_x1e7, uint8_t sats);

// Current filename
const char* wakeLoggerCurrentFile();