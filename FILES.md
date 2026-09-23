# Bike-Mate — File Index

One entry per file. Four lines each: Purpose / Key vars / Key funcs / Notes.

---

### bike_mate.ino
- **Purpose:** Main coordinator — setup, loop, state machine, sleep, upload trigger.
- **Key vars:** macTimeEpoch, totalSeconds, engineWasRunning, accState, inPanic, wifiActive, uploadRequested, uploadCycleCounter.
- **Key funcs:** setup, loop, updateStateTransitions, doStateWork, shouldSleep, goToSleep, currentEpoch, currentWakeMs, currentWakeMode, currentStateString, tprint.
- **Notes:** Owns RTC state. Cold boot resets. n=2 arming trigger sets uploadRequested.

### Config.h
- **Purpose:** Single source of truth — pins, UUIDs, thresholds, structs, feature flags.
- **Key vars:** BIKE_MATE_VERSION, BENCH_MODE, UPLOAD_ENABLED, SDA_PIN=8, SCL_PIN=9, VOLTAGE_PIN=0, THERMISTOR_PIN=3, ACC_LED_PIN=1, BUZZER_PIN=4, STATUS_LED_PIN=10.
- **Key defines:** RideRow, RideSummary, WakeSample, V_RUNNING_ENTER=13.8, V_RUNNING_EXIT=13.0, V_PANIC_ENTER=12.0, V_PANIC_EXIT=12.4, WARN_EMAIL_VOLTAGE=12.50.
- **Notes:** Includes Config.local.h if present. Secrets never committed.

### Config.local.h
- **Purpose:** Local-only secrets — WiFi SSID/password, Gmail credentials.
- **Key vars:** WIFI_SSID, WIFI_PASSWORD, GMAIL_SENDER, GMAIL_APP_PW, MAIL_RECIPIENT.
- **Key funcs:** none (defines only).
- **Notes:** Gitignored. Not on GitHub.

### Sensors.cpp / Sensors.h
- **Purpose:** Read battery voltage and temperature from ADC on GPIO 0 and GPIO 3.
- **Key vars:** latestBatteryVoltage, latestTemperatureC, latestRawVoltage, latestMilliVolts, BATTERY_SLOPE=0.008058f, MEDIAN_SAMPLES=21.
- **Key funcs:** readSensors, medianRaw.
- **Notes:** 21-sample median filter on raw ADC. Battery = raw × BATTERY_SLOPE. NTC not yet read.

### RideLogger.cpp / RideLogger.h
- **Purpose:** Ride lifecycle — start, close, build RideSummary, write to NVS.
- **Key vars:** isLogging, rideStartEpoch, lastLogEpoch, currentRowCount, rideMinV, rideMaxV, rideSumV, rideMinT, rideMaxT, rideUnderSecs, rideOverSecs.
- **Key funcs:** loadRideState, startRideLog, writeRideRow, closeRideLog, setRideStartLocation.
- **Notes:** NVS key s<epoch> holds RideSummary. newest_epoch written at close.

### RideStorage.cpp / RideStorage.h
- **Purpose:** LittleFS ride file lifecycle — create, append, close, delete. Single filename builder.
- **Key vars:** _rideFile, _currentFile[40], _currentEpoch.
- **Key funcs:** rideStorageBuildFilename, rideStorageCreate, rideStorageAppendRow, rideStorageClose, rideStorageDelete, rideStorageExists, rideStorageCurrentFile.
- **Notes:** Filename format ride_YYYYMMDDHHMMSS.csv. Only place filenames are constructed.

### WakeLogger.cpp / WakeLogger.h
- **Purpose:** 24/7 wake log to LittleFS. Rotates on upload.
- **Key vars:** WAKE_FILE_PATH, _lat_x1e7, _lon_x1e7, _sats.
- **Key funcs:** wakeLoggerInit, wakeLoggerTick, wakeLoggerForceWrite, wakeLoggerPause, wakeLoggerResume, wakeLoggerForceRotate, wakeLoggerFileSize, wakeLoggerSetLocation, wakeLoggerLogUploadStart/LogUpload/LogUploadDone.
- **Notes:** File /wakes_YYYY-MM-DD.csv. Sealed as .sealed on rotation.

### BleManager.cpp / BleManager.h
- **Purpose:** BLE server, push protocol, OTA payload parsing.
- **Key vars:** pushRequested, pushInProgress, pushGuiEpoch, bleInited, pServer, pDataChar, pTimeChar, pStreamChar, pRequestChar, pOtaChar.
- **Key funcs:** bleInit, bleStop, bleStart, pushNewSlots, publishBLE, isActuallyConnected.
- **Notes:** UUIDs from Config.h. pushNewSlots re-asserts TZ. OtaCallbacks parses ver from JSON.

### DisplayManager.cpp / DisplayManager.h
- **Purpose:** OLED rendering, edge-triggered refresh, screen dispatch.
- **Key vars:** display, lastOLEDState[48].
- **Key funcs:** drawOLED, updateOLED_EdgeTriggered, drawUploadScreen, drawFwUpdateScreen, drawPanicScreen, drawCountdownScreen, drawArmingScreen, drawRunningScreen.
- **Notes:** 128×64 SSD1306. OTA takes priority over upload screen. Idle → display off.

### WifiManager.cpp / WifiManager.h
- **Purpose:** Unified WiFi bring-up. IDF teardown + re-init. Explicit SSID/password from Config.
- **Key vars:** wifiMessage[24].
- **Key funcs:** wifiBringUp, wifiBringDown, wifiPingTest, ntpSync.
- **Notes:** Sets SSID/password explicitly from Config.h — overrides stale NVS. TX power 8.5dBm (raw 34).

### WifiMail.cpp / WifiMail.h
- **Purpose:** Gmail SMTP send — low-batt alerts, OTA confirmation, ride summaries.
- **Key vars:** _lastMailSentEpoch, _mailFailCount, mailClient.
- **Key funcs:** sendMail, sendLowBatteryAlert, sendRideSummary, sendWeeklySummary, b64, readResp, sendCmd.
- **Notes:** Port 465. Rate-limited by MAIL_RATE_LIMIT_SEC. Backoff at MAIL_FAIL_BACKOFF.

### DriveUpload.cpp / DriveUpload.h
- **Purpose:** POST wake + ride files to Google Apps Script. Delete on success.
- **Key vars:** uploadNames[64][48].
- **Key funcs:** driveUploadPerform, driveUploadShouldRun, uploadAllWakeFiles, uploadAllRideFiles, postFile, urlEncode.
- **Notes:** Two-phase enum. Skips currently-open ride. 302 = success. Draws upload screen.

### OtaManager.cpp / OtaManager.h
- **Purpose:** Download firmware from URL, flash, verify MD5, reboot.
- **Key vars:** otaRequest, otaUrl[], otaSize, otaMd5[], otaVersion[16], otaProgressBytes, otaProgressTotal, otaStage, otaRebootCountdown.
- **Key funcs:** otaPerformUpdate, otaInProgress.
- **Notes:** Stage tracking for OLED. 5-second countdown before reboot. Screen blanks then reboot.

### Buzzer.cpp / Buzzer.h
- **Purpose:** Buzzer tones + status LED mirror.
- **Key vars:** alarmSequenceActive, alarmStep, alarmStepTimer, lowBattBeepActive, lowBattBeepStep, lowBattBeepRemaining, lowBattBeepTimer.
- **Key funcs:** buzzerOn, buzzerOff, updateBeeps.
- **Notes:** LEDC channel 0, 2 kHz. buzzerOn/Off also drives STATUS_LED_PIN.

### bikemate.py
- **Purpose:** Python/Tkinter GUI — BLE client, OTA trigger, Drive sync, two local servers.
- **Key vars:** GUI_VERSION, latest_data, latest_ride, weather, ota_server_proc, drive_server_proc, UPLOAD_URL, OTA_DIR, DRIVE_DIR, SSL_CTX, BACKUP_DELAY_SEC.
- **Key funcs:** BLEWorker, App.menu_ota, App.menu_sync, start_ota_server, start_drive_server, sync_from_drive, backup_firmware_to_drive, parse_summary, parse_row.
- **Notes:** Two HTTP servers (8000 OTA, 8001 Drive). OTA uses Familys-iMac.local. 10-min delay before Drive backup.

### README.md
- **Purpose:** GitHub landing page — features, hardware, build instructions.
- **Notes:** STALE — currently says V2.00, describes old 72×40 board. Needs rewrite for V4.04.

### HANDOFF.md
- **Purpose:** Session handoff doc — git state, file list, open items.
- **Notes:** Regenerated by update_handoff.sh.

### PROJECT_STATE.md
- **Purpose:** Project snapshot — versions, hardware, what's next.
- **Notes:** Updated by hand.

### FILES.md (this file)
- **Purpose:** One-entry-per-file index. Purpose, key vars, key funcs, notes.

### update_handoff.sh
- **Purpose:** Regenerates HANDOFF.md from git state and file list.
- **Notes:** Shell script. Run manually.
