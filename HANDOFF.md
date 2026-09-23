# Bike-Mate — Chat Handoff

Generated: 2026-09-22 14:10:15

---

## GIT STATE

```
$ git log --oneline -5
6608259 Update PROJECT_STATE.md to V3.29
02c6db0 V3.29: end-to-end Drive upload working
03dfedc V3.01: new hardware + GPS stub + BLE MTU fix
5deb93e Update PROJECT_STATE to V2.72 + GUI V2.31
9fa15de V2.72 firmware + GUI V2.31: immediate push + 48-byte RideSummary

$ git status --short
 M Config.h
 M DisplayManager.cpp
 M DriveUpload.cpp
 M WakeLogger.cpp
 M WifiManager.cpp
 M WifiManager.h
 M bike_mate.ino
?? HANDOFF.md
?? update_handoff.sh

$ git tag -l | tail -10
gui-v2.30
gui-v2.31
snapshot-2026-09-21
snapshot-2026-09-22
v2.50
v2.63
v2.70
v2.72
v3.01
v3.29
```

---

## PROJECT_STATE.md

# Bike-Mate — Project State

Snapshot date: 2026-09-22
Latest firmware on bike: V3.29
Latest firmware tag: v3.29
Latest GUI: v2.31
Repo: https://github.com/bigbadevilaussie-hue/bikemate-firmware

---

## HARDWARE (current bench + target)

### Bench setup (V3.00 dev kit)
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, addr 0x3C)
- Breadboard with jumpers
- USB-C power from Mac

### Bike target (V3.00 perfboard, not yet built)
- XCW ESP32-C3 SuperMini
- External 0.96" OLED
- MF52AT NTC 10K thermistor
- Mini 360 buck (12V -> 5V)
- SA16CA TVS diode
- IRF4905 P-MOSFET + PN2222A NPN (ACC switch)
- QC3.0 USB buck (charger module)
- 12V rocker switch
- NEO-6M GPS (on order)
- Perfboard

### Old bike board (retired / on loan)
- ESP32-C3 SuperMini with built-in 72x40 OLED
- Was running V2.72
- Library: Adafruit_SSD1306_72x40 (custom)

## PIN MAP (V3.00)

| Function | GPIO |
|---|---|
| Voltage sense | 0 |
| ACC LED | 1 |
| Thermistor | 3 |
| Buzzer | 4 |
| OLED SDA | 8 |
| OLED SCL | 9 |
| Status LED | 10 |

V2.x (old board): SDA=5, SCL=6

## FIRMWARE VERSIONS

| Version | Contents |
|---|---|
| V2.00 | Modular port |
| V2.10Burn | Fake voltage burn-in, 4-test cycle |
| V2.50 | Mail alerts via Gmail SMTP |
| V2.61 | Firmware version over BLE status JSON |
| V2.62 | OTA works end-to-end |
| V2.63 | Remote OTA verified |
| V2.70 | Wake + ride logging on LittleFS, WakeLogger + RideStorage |
| V2.72 | Immediate push, 15s sleep cap test |
| V3.00 | XCW SuperMini + external 128x64 OLED, GPS stub |
| V3.01 | Standard Adafruit_SSD1306, 128x64 layout, BLE JSON trim |
| V3.02 | Date-keyed wake files, Drive upload at 04:00 (planned) |
| V3.11 | Unified WiFi bring-up, BLE stops during WiFi |
| V3.12 | REC dot blinks at 1Hz while logging |
| V3.16 | TX power 8.5dBm fix for connect failure |
| V3.17 | MONITOR-only upload, bench force-rotate |
| V3.18 | [BOOT] cycleCount, [UPLOAD-CHK] diagnostics |
| V3.19 | firstTick (doStateWork on wake), 60s sleep cap |
| V3.27 | LittleFS path slash fix |
| V3.28 | Treated 302 as success (planned) |
| V3.29 | End-to-end Drive upload working |

## MODULES

| File | Purpose |
|---|---|
| bike_mate.ino | Coordinator, state machine, sleep, WiFi trigger |
| Config.h | All #defines, thresholds, structs, upload URL |
| Sensors.h/.cpp | ADC reads |
| Buzzer.h/.cpp | Alarm + beep sequences |
| RideLogger.h/.cpp | Ride start/close, summary build |
| RideStorage.h/.cpp | LittleFS ride file writer (/ride_YYYYMMDDHHMM.csv) |
| WakeLogger.h/.cpp | LittleFS wake log (/wakes_YYYY-MM-DD.csv) |
| BleManager.h/.cpp | BLE server, characteristics, bleStop/bleStart |
| DisplayManager.h/.cpp | OLED rendering, REC blink |
| WifiManager.h/.cpp | Unified WiFi bring-up (TX power, NTP, ping) |
| DriveUpload.h/.cpp | Upload wake + ride files to Google Drive |
| WifiMail.h/.cpp | Gmail SMTP alerts |
| OtaManager.h/.cpp | HTTP OTA fetch + flash |
| bikemate.py | Python GUI (V2.31) |

## PROTECTED SYSTEMS

Do not casually modify:
- **OLED** — pins (8, 9), library (Adafruit_SSD1306), init, timing
- **Sleep/wake** — wake intervals, RTC state, sleep gate
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify protocol
- **WiFi bring-up** — TX power must stay 13dBm on XCW board
- **Upload pipeline** — 302 handling, URL-encoded POST format

## V3.29 — CURRENT

### Working
- WiFi connect on ESP32-C3 SuperMini (TX power 13dBm fix)
- BLE + GUI + time sync + ride push
- Wake logging to /wakes_YYYY-MM-DD.csv
- Ride logging to /ride_YYYYMMDDHHMM.csv
- Drive upload at 04:00 local / every N wakes in bench
- Apps Script writes files to Google Drive
- 20 verified successful uploads
- Drive folder has wakes_2026-09-21.csv (8KB), wakes_2026-09-22.csv, all rides

### Configuration
- BENCH_MODE=1: wake every 30s
- BENCH_MODE=0: day 5min, night 10min
- UPLOAD_BENCH_TRIGGER_EVERY_N_WAKES=1 (test)
- WIFI_SSID/WIFI_PASSWORD: see Config.local.h
- UPLOAD_URL: (new deployment URL in Config.h)

## GOOGLE APPS SCRIPT

URL: (see Config.h UPLOAD_URL)
Deployment ID: AKfycbw6SyEUOhDU6cJE748IUjYHhF2uNd9wyl3Z5AAaWWPczTwC9CH4KSznY2Gifr0r_403
Method: POST with form fields filename and data
Response: 302 redirect (treated as success by ESP32)

## GOOGLE DRIVE

Folder: https://drive.google.com/drive/folders/1I48SYu8vTC4CDTFLUhZh53siI8ULbQ8W
Contents: wakes_*.csv (one per day) + ride_*.csv (one per ride)

## WHAT'S NEXT (V3.30+)

1. **Fix 302-delete** — change DriveUpload.cpp to set ok=true on 302
   so local files get removed after upload
2. **04:00 field test** — confirm daily upload trigger
3. **Wake file rotation at 04:00** — verify date-keyed rollover
4. **NEO-6M GPS** — when hardware arrives, replace GPS stub
5. **Perfboard build** — soldered V3.00
6. **report-mate.py** — post-process Drive CSVs

## KNOWN ISSUES

- 302 no Location — ESP32 can't read redirect, treats as failure
  (files stay on device, re-upload next cycle, Drive overwrites)
- NTP sometimes slow (up to 8s)
- WiFi RSSI marginal (-47 to -65 dBm depending on position)
- Empty ride files occasionally created (0 bytes)

## BOM STATUS

Arrived:
- XCW ESP32-C3 SuperMini
- 0.96" OLED 128x64
- T12 soldering station
- Perfboard

On order:
- NEO-6M GPS
- IRF4905 MOSFET
- PN2222A NPN
- MF52AT NTC thermistor
- Mini 360 buck
- SA16CA TVS
- QC3.0 USB buck

## QUICK REFERENCE

Build: Arduino IDE 2.x, ESP32 core 2.0.17 (2.x required)
Partition: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
Serial: 115200
Upload: Sketch -> Upload (or Export Compiled Binary + OTA)

Wake file format:
  # fw=X.XX
  # opened=<epoch> (YYYY-MM-DD HH:MM)
  epoch,lat,lon,volt,temp,state,flags,sats

Ride file format:
  # fw=X.XX
  # ride_start=<epoch> (YYYY-MM-DD HH:MM)
  epoch,lat,lon,volt,temp,state

BLE JSON (97 bytes, fits MTU 101):
  {"v":..,"t":..,"a":..,"e":..,"w":..,"s":"..","p":..,"ll":"lat,lon","lg":..,"gs":..}

---

End of snapshot.

---

## CURRENT FILE LIST

```
-rw-r--r--  1 Nick  staff  10473 22 Sep 00:34 BleManager.cpp
-rw-r--r--@ 1 Nick  staff    781 21 Sep 22:10 BleManager.h
-rw-r--r--@ 1 Nick  staff   1342 21 Sep 19:51 Buzzer.cpp
-rw-r--r--@ 1 Nick  staff    331 21 Sep 19:51 Buzzer.h
-rw-r--r--  1 Nick  staff   4992 22 Sep 08:52 Config.h
-rw-r--r--@ 1 Nick  staff   6451 22 Sep 08:52 DisplayManager.cpp
-rw-r--r--@ 1 Nick  staff    283 21 Sep 19:51 DisplayManager.h
-rw-r--r--  1 Nick  staff   8461 22 Sep 08:54 DriveUpload.cpp
-rw-r--r--  1 Nick  staff    782 21 Sep 20:53 DriveUpload.h
-rw-r--r--@ 1 Nick  staff   3598 21 Sep 22:15 OtaManager.cpp
-rw-r--r--@ 1 Nick  staff    785 21 Sep 19:51 OtaManager.h
-rw-r--r--@ 1 Nick  staff   4938 21 Sep 19:51 RideLogger.cpp
-rw-r--r--@ 1 Nick  staff    997 21 Sep 19:51 RideLogger.h
-rw-r--r--  1 Nick  staff   3570 22 Sep 00:33 RideStorage.cpp
-rw-r--r--  1 Nick  staff   1193 21 Sep 19:51 RideStorage.h
-rw-r--r--@ 1 Nick  staff   3231 21 Sep 19:51 Sensors.cpp
-rw-r--r--@ 1 Nick  staff    387 21 Sep 19:51 Sensors.h
-rw-r--r--  1 Nick  staff   7619 22 Sep 08:55 WakeLogger.cpp
-rw-r--r--  1 Nick  staff   1375 22 Sep 00:00 WakeLogger.h
-rw-r--r--@ 1 Nick  staff   5946 21 Sep 22:16 WifiMail.cpp
-rw-r--r--@ 1 Nick  staff    634 21 Sep 19:51 WifiMail.h
-rw-r--r--  1 Nick  staff   3072 22 Sep 08:53 WifiManager.cpp
-rw-r--r--  1 Nick  staff    373 22 Sep 08:06 WifiManager.h
-rw-r--r--@ 1 Nick  staff  16995 22 Sep 08:17 bike_mate.ino
-rw-r--r--  1 Nick  staff  27971 21 Sep 21:55 bikemate.py
```

---

## LATEST SERIAL SNAPSHOT (optional — paste manually)

_Paste last serial output here if needed._

