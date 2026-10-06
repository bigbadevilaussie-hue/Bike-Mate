# Bike-Mate — File Index

One entry per file. Purpose, key vars, key funcs, notes.

---

## Firmware

### bike_mate.ino
- **Purpose:** Main coordinator — setup, loop, state machine, sleep, upload trigger, maint loop.
- **Key vars:** macTimeEpoch, totalSeconds, engineWasRunning, accState, inPanic, wifiActive, uploadRequested, maintRequest, maintStartMs.
- **Key funcs:** setup, loop, updateStateTransitions, doStateWork, shouldSleep, goToSleep, currentEpoch, currentWakeMs, currentWakeMode, currentStateString, tprint, tprint_verbose.
- **Notes:** Owns RTC state. Cold boot resets. Consumes maintRequest on wake — see MAINTENANCE.md. Maint block lives in loop() and runs the HTTP server inline.

### Config.h
- **Purpose:** Single source of truth — pins, UUIDs, thresholds, structs, feature flags, version.
- **Key vars:** BIKE_MATE_VERSION, BENCH_MODE, MAINT_MAX_MS, MAX_AWAKE_MS, BLE_MAX_CONN_MS, V_PANIC_ENTER/EXIT, WARN_EMAIL_VOLTAGE.
- **Notes:** Includes Config.local.h if present. Secrets never committed.

### Config.local.h
- **Purpose:** Local-only secrets — WiFi SSID/password, Gmail credentials.
- **Notes:** Gitignored. Not on GitHub.

### Sensors.cpp / Sensors.h
- **Purpose:** Read battery voltage and temperature from ADC on GPIO 0 and GPIO 3.
- **Key vars:** latestBatteryVoltage, latestTemperatureC, latestRawVoltage, BATTERY_SLOPE, MEDIAN_SAMPLES.
- **Key funcs:** readSensors, medianRaw.

### RideLogger.cpp / RideLogger.h
- **Purpose:** Ride lifecycle — start, close, build RideSummary, write to NVS.
- **Key vars:** isLogging, rideStartEpoch, currentRowCount, rideMinV/MaxV/SumV, rideMinT/MaxT.
- **Key funcs:** loadRideState, startRideLog, writeRideRow, closeRideLog, setRideStartLocation.

### RideStorage.cpp / RideStorage.h
- **Purpose:** LittleFS ride file lifecycle.
- **Key funcs:** rideStorageBuildFilename, rideStorageCreate, rideStorageAppendRow, rideStorageClose, rideStorageDelete, rideStorageExists.

### WakeLogger.cpp / WakeLogger.h
- **Purpose:** 24/7 wake log to LittleFS. Date-keyed. Rotates on upload.
- **Key funcs:** wakeLoggerInit, wakeLoggerTick, wakeLoggerForceWrite, wakeLoggerPause, wakeLoggerResume, wakeLoggerForceRotate, wakeLoggerSetLocation.

### BleManager.cpp / BleManager.h
- **Purpose:** BLE server. Telemetry push. Maint trigger. Settings read/apply.
- **Key vars:** pushRequested, pushGuiEpoch, bleInited, pServer, pDataChar, pTimeChar, pStreamChar, pRequestChar, pOtaChar, pSettingsChar.
- **Key funcs:** bleInit, bleStop, bleStart, pushNewSlots, publishBLE, isActuallyConnected.
- **Notes:** OtaCallbacks::onWrite parses {"maint":"on"} and sets maintRequest. Also handles settings read/write.

### DisplayManager.cpp / DisplayManager.h
- **Purpose:** OLED rendering.
- **Key funcs:** drawOLED, updateOLED_EdgeTriggered, drawMaintScreen, drawUploadScreen, drawFwUpdateScreen, drawPanicScreen, drawRunningScreen.

### WifiManager.cpp / WifiManager.h
- **Purpose:** Unified WiFi bring-up. IDF teardown + re-init. Explicit SSID/password.
- **Key funcs:** wifiBringUp, wifiBringDown, wifiPingTest, ntpSync.
- **Notes:** Calls bleStop() at top of bring-up, bleStart() at end of bring-down.

### WifiMail.cpp / WifiMail.h
- **Purpose:** Gmail SMTP send — low-batt alerts, PANIC alerts.
- **Key funcs:** sendMail, sendLowBatteryAlert, b64, readResp, sendCmd.

### DriveUpload.cpp / DriveUpload.h
- **Purpose:** POST wake + ride files to Google Apps Script. Delete on success.
- **Key funcs:** driveUploadPerform, driveUploadShouldRun, uploadAllWakeFiles, uploadAllRideFiles, postFile, urlEncode.

### OtaManager.cpp / OtaManager.h
- **Purpose:** Download firmware from URL, flash, verify MD5, reboot.
- **Notes:** Invoked from doStateWork when otaRequest is true. Legacy — HTTP OTA in maint is the current flow.

### Settings.cpp / Settings.h
- **Purpose:** Runtime-tunable values in NVS bikeset namespace.
- **Key funcs:** settingsLoad, settingsSave, settingsApplyJson, settingsToJson.

### Buzzer.cpp / Buzzer.h
- **Purpose:** Buzzer tones + status LED mirror.
- **Key funcs:** buzzerOn, buzzerOff, updateBeeps.

### GpsModule.cpp / GpsModule.h
- **Purpose:** NMEA parser for NEO-6M over UART. No external library.
- **Key funcs:** gpsModuleInit, gpsModuleTick, gpsHasFix, gpsLat_x1e7, gpsLon_x1e7, gpsSats, gpsSpeed_kmh.

### SerialBuffer.cpp / SerialBuffer.h
- **Purpose:** Ring buffer feeding /serial-raw web page.
- **Key vars:** SERIAL_BUF_LINES = 50.
- **Key funcs:** serialBufInit, serialBufWrite, serialBufGet.
- **Notes:** tprint() writes each complete line (timestamp + message + newline) to the buffer.

### WebServer.cpp / WebServer.h
- **Purpose:** HTTP server for maintenance mode.
- **Key funcs:** serverSetup, serverLoop, serverStop, serverIsRunning.
- **Routes:** /serial, /serial-raw, /maint/off, /ota, /version, /ota-progress.
- **Key vars:** maintOffRequested.
- **Notes:** Only live during maintenance mode.

---

## GUI

### bikemate.py
- **Purpose:** Python/Tkinter GUI — BLE client, HTTP OTA, Drive sync, two local HTTP servers.
- **Key vars:** GUI_VERSION, maintenance_state, maintenance_lock, BIKE_IP, latest_data, latest_ride, weather.
- **Key funcs:** BLEWorker, App.activate_maintenance, App.deactivate_maintenance, App._start_maint_poll, App._start_maint_off_poll, App._check_maint_on_startup, App.menu_ota, App.menu_sync.
- **Notes:** Maint state machine: OFF / PENDING / ON. HTTP OTA gated on maint ON. Startup recovery probe.

---

## Docs

### README.md
- Public-facing overview. HTTP endpoint list. Maint mode summary.

### PROJECT_STATE.md
- Current snapshot: hardware, modules, endpoints, priorities, protected systems.

### FILES.md (this file)
- One-entry-per-file index.

### HANDOFF.md
- Session handoff: git state, versions, rules, architecture, what's next.

### MAINTENANCE.md
- Maint mode architecture: entry/exit, safety cap, serial page, HTTP OTA, GUI state machine.

### BENCH.md
- Physical breadboard, traced circuits, hardware TODO.

### TODO.md
- Ranked action list, tiers 1-5.

### AUDIT.md
- Historical — four-AI peer review from 2026-10-01.

---

## Build

### update_handoff.sh
- **Purpose:** Regenerates HANDOFF.md from git state and file list.
- **Notes:** Run manually.

---

*If this file and the code disagree, the code wins. Fix this file.*
