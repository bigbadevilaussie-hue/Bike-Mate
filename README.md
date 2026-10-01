# Bike-Mate

Motorcycle battery, temperature and ride logger. ESP32-C3 + BLE + OLED.

**Prime directive: Bike-Mate must not drain the bike battery past the point
where it will not crank the engine.**

---

## Current status

- **Firmware:** see `Config.h` → `BIKE_MATE_VERSION`
- **GUI:** see `bikemate.py` → `GUI_VERSION`
- **Hardware:** breadboard (perfboard pending)
- **Bike:** Triumph Sprint ST 1050, stock YT12B-BS AGM (~10Ah)
- **Latest peer review:** see `AUDIT.md` (four independent AI reviewers, 2026-10-01)
- **Open work:** see `TODO.md`

---

## Hardware

### Current bench
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, addr 0x3C)
- MP1584EN buck module (12V → 5V) — quiescent current unmeasured
- 12V bench PSU (30V / 3A)
- Breadboard + jumpers
- 12V voltage divider (100k / 10k) on GPIO 0 — **no filter cap yet**
- MF52AT NTC 10k (Beta 3950) on GPIO 3, with 100nF ceramic cap
- PN2222A driving a switched 12V rail (for GPS + USB charger) — **LED simulation only**
- GY-NEO6M V2 NEO-6M GPS — **not wired**
- IRF4905 P-MOSFET + PN2222A — **not wired**

### Pin map (authoritative — from Config.h)
| Function | GPIO |
|---|---|
| Voltage sense | 0 |
| ACC LED / MOSFET gate | 1 |
| Thermistor | 3 |
| Buzzer | 4 |
| OLED SDA | 6 |
| OLED SCL | 9 |
| Status LED | 10 |

Note: SCL on GPIO9 is a boot-strapping pin on the ESP32-C3. Do not add strong
external pull-downs or large capacitors to this line.

---

## Firmware

- **Framework:** Arduino IDE 2.x, ESP32 core 2.0.17 (2.x required — 3.x not supported)
- **Partition:** Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)
- **Serial:** 115200
- **BENCH_MODE** in `Config.h` controls wake interval:
  - 1 = 30s wakes (bench/debug only — **do not deploy**)
  - 0 = 300s day / 600s night (field)

### Modules
| File | Purpose |
|---|---|
| `bike_mate.ino` | Coordinator: setup, loop, state machine, sleep, WiFi trigger |
| `Config.h` | Pins, UUIDs, thresholds, structs, URLs |
| `Sensors.cpp` | ADC reads for voltage and thermistor |
| `Buzzer.cpp` | Alarm + low-batt beep sequences |
| `RideLogger.cpp` | Ride lifecycle, summary to NVS |
| `RideStorage.cpp` | LittleFS ride CSV writer |
| `WakeLogger.cpp` | LittleFS wake CSV writer (date-keyed) |
| `BleManager.cpp` | BLE server, push protocol, OTA payload parsing |
| `DisplayManager.cpp` | OLED rendering, edge-triggered updates |
| `WifiManager.cpp` | Unified WiFi bring-up (IDF teardown + re-init) |
| `WifiMail.cpp` | Gmail SMTP alerts |
| `DriveUpload.cpp` | Wake + ride CSV upload to Google Drive via Apps Script |
| `OtaManager.cpp` | HTTP OTA fetch + flash |

---

## GUI

- **Location:** `bikemate.py` (also on GitHub)
- **Requires:** `bleak`, Python 3.8+
- **Features:** BLE live telemetry, ride pull, OTA trigger, Drive sync,
  two local HTTP servers (OTA staging + Drive mirror)

---

## Data flow

1. Wake on timer (or BLE event)
2. Read sensors → update state machine
3. Log a wake row to `/wakes_YYYY-MM-DD.csv`
4. During rides, log rows to `/ride_YYYYMMDDHHMMSS.csv` every `LOG_INTERVAL_SEC`
5. On ride close, write summary to NVS
6. On upload trigger (04:00 or bench), POST files to Apps Script → Google Drive
7. On successful upload, delete local file

---

## Documentation in this repo

| File | Purpose |
|---|---|
| `README.md` | This file — overview |
| `PROJECT_STATE.md` | Current snapshot: versions, hardware, open items |
| `HANDOFF.md` | Session handoff: git state, file list |
| `FILES.md` | One-entry-per-file index |
| `AUDIT.md` | Consolidated peer review from four AI reviewers (2026-10-01) |
| `TODO.md` | Ranked action list, by verifiability |

If `README.md`, `PROJECT_STATE.md`, `HANDOFF.md`, or `FILES.md` disagree
with `Config.h` or the `.cpp`/`.ino` files, **the code wins**. Fix the docs.

---

## Protected systems (do not casually modify)

- **OLED** — pins (6, 9), library, init sequence, draw timing, edge-trigger logic
- **Sleep/wake** — wake intervals, RTC state, cold-boot reset, sleep gate conditions
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify timing, push protocol
- **WiFi bring-up** — TX power (currently 8.5 dBm), NTP gating
- **Upload pipeline** — 302 handling, URL-encoded POST format

Any change to these must be tested end-to-end on real hardware.

---

## Known issues and priorities

See `AUDIT.md` for the full list and `TODO.md` for the ranked plan.

Top of the list:
1. `BENCH_MODE 1` is compiled in — do not deploy
2. PANIC state blocks sleep (prime-directive conflict)
3. `readSensors()` early-return leaves stale voltage on fault
4. OTA MD5 verified after `Update.end(true)`
5. Panic threshold (12.0V) below stated crank floor (12.2V)

---

## Safety

`Config.local.h` contains WiFi and Gmail credentials. It is gitignored.
Never commit it. Never paste a dump that includes it.
