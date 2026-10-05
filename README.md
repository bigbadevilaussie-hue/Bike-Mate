# Bike-Mate

Motorcycle battery, temperature and ride logger. ESP32-C3 + BLE + OLED.

**Prime directive: Bike-Mate must not drain the bike battery past the point
where it will not crank the engine.**

---

## Current status

- **Firmware:** see `Config.h` → `BIKE_MATE_VERSION`
- **GUI:** see `bikemate.py` → `GUI_VERSION`
- **Hardware:** breadboard (perfboard pending)
- **Bike:** Triumph Sprint ST 1050, SSB Powersport VTX12-BS AGM
- **Open work:** see `TODO.md`

If this file and the code disagree, the code wins. Fix the docs.

---

## Hardware

### Current bench
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, addr 0x3C)
- MP1584EN buck module (12V → 5V)
- 12V bench PSU (30V / 3A)
- Breadboard + jumpers
- 12V voltage divider (98.8k / 9.98k) on GPIO 0
- MF52AT NTC 10k (Beta 3950) on GPIO 3
- PN2222A + IRF4905 switched 12V rail — breadboard only
- GY-NEO6M V2 NEO-6M GPS — not yet wired

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
| GPS RX / TX | 2 / 21 |

Note: SCL on GPIO9 is a boot-strapping pin on the ESP32-C3. Do not add strong
external pull-downs or large capacitors to this line.

---

## Firmware

- **Framework:** Arduino IDE 2.x, ESP32 core 2.0.17 (2.x required — 3.x not supported)
- **Partition:** Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)
- **Build:** `arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs" .`
- **Serial:** 115200
- **BENCH_MODE** in `Config.h` controls wake interval:
  - 1 = 30s wakes (bench/debug only — do not deploy)
  - 0 = 300s day / 600s night (field)

### Modules
| File | Purpose |
|---|---|
| `bike_mate.ino` | Coordinator: setup, loop, state machine, sleep, WiFi trigger |
| `Config.h` | Pins, UUIDs, thresholds, structs, feature flags |
| `Sensors.cpp` | ADC reads for voltage and thermistor |
| `Buzzer.cpp` | Alarm + low-batt beep sequences |
| `RideLogger.cpp` | Ride lifecycle, summary to NVS |
| `RideStorage.cpp` | LittleFS ride CSV writer |
| `WakeLogger.cpp` | LittleFS wake CSV writer (date-keyed) |
| `BleManager.cpp` | BLE server, telemetry push, maint trigger |
| `DisplayManager.cpp` | OLED rendering, edge-triggered updates |
| `WifiManager.cpp` | Unified WiFi bring-up (IDF teardown + re-init) |
| `WifiMail.cpp` | Gmail SMTP alerts |
| `DriveUpload.cpp` | Wake + ride CSV upload to Google Drive via Apps Script |
| `OtaManager.cpp` | HTTP OTA fetch + flash (invoked from maintenance mode) |
| `Settings.cpp` | Runtime-tunable values in NVS |
| `GpsModule.cpp` | NMEA parser, no external library |
| `SerialBuffer.cpp` | Ring buffer feeding the /serial web page |
| `WebServer.cpp` | HTTP server: /serial, /serial-raw, /maint/off, /ota, /version |

### HTTP endpoints (live during maintenance mode only)
| Route | Method | Purpose |
|---|---|---|
| `/serial` | GET | HTML serial page (live tail of tprint output) |
| `/serial-raw` | GET | Plain text serial dump (ring buffer) |
| `/maint/off` | POST | Exit maintenance mode |
| `/ota` | POST | Firmware upload (multipart) |
| `/version` | GET | Current BIKE_MATE_VERSION as text |

---

## Maintenance mode

The bike sleeps most of the time. To do anything interactive — read serial,
flash firmware — the bike must be woken into **maintenance mode**: WiFi up,
HTTP server running, BLE off, awake for a bounded window.

Full detail in `MAINTENANCE.md`.

Quick version:
- **Enter:** GUI menu → Activate Maintenance Mode. GUI sends `{"maint":"on"}`
  over BLE. Firmware ACKs, sets a flag, goes back to sleep. On next wake,
  the flag triggers WiFi + HTTP server.
- **Exit:** click **End Maintenance** on the serial page, or GUI menu →
  Deactivate. Firmware POSTs `/maint/off` to itself, tears WiFi down, sleeps.
- **Safety cap:** `MAINT_MAX_MS` (15 min) — firmware force-exits maint and
  sleeps even if nobody tells it to.

OTA happens inside maint mode. The GUI POSTs the compiled binary to
`/ota`; the firmware streams it into `Update.h` and reboots.

---

## GUI

- **Location:** `bikemate.py` (single file, Tk)
- **Requires:** `bleak`, `requests`, Python 3.8+
- **Features:** BLE live telemetry, ride pull, maintenance mode control,
  serial page launch, HTTP OTA, Drive sync, two local HTTP servers
  (OTA staging + Drive mirror)

---

## Data flow

1. Wake on timer (or BLE event)
2. Read sensors → update state machine
3. Log a wake row to `/wakes_YYYY-MM-DD.csv`
4. During rides, log rows to `/ride_YYYYMMDDHHMMSS.csv`
5. On ride close, write summary to NVS
6. On upload trigger (04:00 or bench), POST files to Apps Script → Google Drive
7. On successful upload, delete local file

---

## Documentation in this repo

| File | Purpose |
|---|---|
| `README.md` | This file — overview |
| `PROJECT_STATE.md` | Current snapshot: versions, hardware, open items |
| `FILES.md` | One-entry-per-file index |
| `HANDOFF.md` | Session handoff: git state, rules, current phase |
| `MAINTENANCE.md` | Maintenance mode + serial page + HTTP OTA architecture |
| `BENCH.md` | Physical breadboard, traced circuits, hardware TODO |
| `TODO.md` | Ranked action list |
| `AUDIT.md` | Historical — four-AI peer review from 2026-10-01 |

---

## Protected systems (do not casually modify)

- **OLED** — pins (6, 9), library, init sequence, draw timing
- **Sleep/wake** — wake intervals, RTC state, cold-boot reset, sleep gates
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify timing
- **WiFi bring-up** — TX power (8.5 dBm on this board), NTP gating
- **Upload pipeline** — 302 handling, URL-encoded POST format

Any change to these must be tested end-to-end on real hardware.

---

## Safety

`Config.local.h` contains WiFi and Gmail credentials. It is gitignored.
Never commit it. Never paste a dump that includes it.
