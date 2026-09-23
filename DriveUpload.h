#pragma once

#include <Arduino.h>
#include "Config.h"

// DriveUpload — V3.02
// At 04:00 local (bench: every N wakes), uploads sealed wake files
// and all ride files (except the newest) to Google Drive via Apps Script.
//
// Pause pattern matches OTA: everything in doStateWork() blocks while
// upload runs. Wake logger is explicitly paused during upload.
//
// Called from bike_mate.ino:
//   if (driveUploadShouldRun()) { driveUploadPerform(); }

// Returns true if an upload should run right now.
// - Bench mode: every UPLOAD_BENCH_TRIGGER_EVERY_N_WAKES wakes
// - Field mode: once per local day, after UPLOAD_HOUR_LOCAL
bool driveUploadShouldRun();

// Performs the upload. Blocks until done.
// Returns true if all files uploaded successfully.
bool driveUploadPerform();