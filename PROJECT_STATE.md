# Bike-Mate — Project State

Snapshot date: 2026-10-05
Firmware: see `Config.h` → `BIKE_MATE_VERSION`
GUI: see `bikemate.py` → `GUI_VERSION`
Repo: https://github.com/bigbadevilaussie-hue/Bike-Mate

---

## PROJECT PHILOSOPHY

Rule 1 — Have fun
Rule 2 — Over engineer it
Rule 3 — It's useless but look cool
Rule 4 — Fuck it lmao

**Prime directive overrides everything else:**
Bike-Mate must not drain the bike battery past the point where it will
not crank the engine. On the Sprint ST 1050 with a VTX12-BS AGM,
the practical cranking floor is ~12.2V rested.

---

## HARDWARE

### Current bench
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, addr 0x3C)
- MP1584EN buck module (12V → 5V)
- 12V bench PSU (30V / 3A)
- Breadboard + jumpers
- PN2222A + IRF4905 switched 12V rail (breadboard, LED simulation only)
- GY-NEO6M V2 NEO-6M GPS (in hand, not wired)

### Analog front-ends
- Voltage divider 98.8k / 9.98k on GPIO 0
- MF52AT NTC 10k (Beta 3950) on GPIO 3, with 100nF ceramic cap

### Digital I/O
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

Note: GPIO9 is a boot-strapping pin on the ESP32-C3.

### Not yet wired on the bike
- TVS diode (SA16CA)
- IRF4905 switched 12V rail (breadboard only)
- GY-NEO6M GPS (in hand, not wired)
- QC3.0 USB charger
- 12V rocker switch
- 12V piezo buzzer

---

## FIRMWARE

**Current version:** see `Config.h` → `BIKE_MATE_VERSION`
**Build partition:** Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)
**Build command:**

### Modules
| File | Purpose |
|---|---|
| `bike_mate.ino` | Coordinator: setup, loop, state machine, sleep, maint consume |
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
| `OtaManager.cpp` | HTTP OTA fetch + flash |
| `Settings.cpp` | Runtime-tunable values in NVS |
| `GpsModule.cpp` | NMEA parser, no external library |
| `SerialBuffer.cpp` | Ring buffer feeding the /serial web page |
| `WebServer.cpp` | HTTP server: /serial, /serial-raw, /maint/off, /ota, /version |

### HTTP endpoints (live during maintenance only)
| Route | Method | Purpose |
|---|---|---|
| `/serial` | GET | HTML serial page |
| `/serial-raw` | GET | Plain text serial dump |
| `/maint/off` | POST | Exit maintenance mode |
| `/ota` | POST | Firmware upload (multipart) |
| `/version` | GET | Current firmware version |

---

## GUI

**Current version:** see `bikemate.py` → `GUI_VERSION`

Single file, Tk. Uses `bleak` for BLE, `requests` for HTTP.

Features:
- BLE live telemetry (v, t, a, e, w, s)
- Ride pull over BLE (STREAM_UUID + REQUEST_UUID)
- Maintenance mode control (Activate / Deactivate)
- Serial page launch on maint entry
- HTTP OTA during maint mode
- Drive sync (pull from Apps Script)
- Two local HTTP servers: port 8000 (OTA staging), port 8001 (Drive mirror)

---

## MAINTENANCE MODE

See `MAINTENANCE.md` for full architecture.

Summary:
- **Enter:** BLE `{"maint":"on"}` → firmware sets `maintRequest = true`, ACKs, sleeps.
  On next wake, `doStateWork` sees the flag, brings WiFi up, starts HTTP server,
  enters maint for `MAINT_MAX_MS` (15 min).
- **Exit:** HTTP `/maint/off`, or 15-min timeout.
- **During maint:** BLE off, WiFi up, serial page live, OTA available.
- **Safety:** `MAINT_MAX_MS` in `Config.h`.

---

## CURRENT STATUS

- Firmware, GUI, BLE, maintenance mode, HTTP OTA, ride logging,
  wake logging, Drive upload, email alerts — all functionally working.
- Deep sleep tested and stable.
- Hardware: breadboard. Perfboard pending.
- GPS not wired. MOSFET ACC switch not wired. TVS not fitted.
- Sleep current not yet measured.

---

## TOP PRIORITIES

See `TODO.md` for the full ranked list. Short version:

1. Set `BENCH_MODE 0` before any deployment
2. Fix `readSensors()` early-return (stale voltage on fault)
3. Measure sleep current (MP1584EN swap if needed)
4. Wire GPS, test fix outdoors
5. Complete the switched 12V rail (MOSFET + TVS + fuse)
6. Field install + 4-week baseline

---

## PROTECTED SYSTEMS

Do not casually modify:

- **OLED** — pins (6, 9), library, init, timing
- **Sleep/wake** — wake intervals, RTC state, sleep gate conditions
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify protocol
- **WiFi bring-up** — TX power must stay 8.5 dBm on this board
- **Upload pipeline** — 302 handling, URL-encoded POST format
- **Maintenance mode** — flag pattern (BLE set, next-wake consume),
  `MAINT_MAX_MS` cap, server lifecycle

Any change to these must be tested end-to-end on real hardware.

---

## SAFETY REMINDERS

- `Config.local.h` contains WiFi + Gmail credentials. Gitignored.
  If it has ever been shared, rotate the credentials.
- Apps Script URL is unauthenticated. Fix before real GPS data lands.
- `BENCH_MODE 1` is compiled in during dev — do not deploy on the bike.

---

## FILES IN REPO

See `FILES.md` for the full index. Quick list:

Firmware: `bike_mate.ino`, `Config.h`, `Sensors.*`, `Buzzer.*`,
`RideLogger.*`, `RideStorage.*`, `WakeLogger.*`, `BleManager.*`,
`DisplayManager.*`, `WifiManager.*`, `WifiMail.*`, `DriveUpload.*`,
`OtaManager.*`, `Settings.*`, `GpsModule.*`, `SerialBuffer.*`,
`WebServer.*`

GUI: `bikemate.py`

Docs: `README.md`, `PROJECT_STATE.md`, `FILES.md`, `HANDOFF.md`,
`MAINTENANCE.md`, `BENCH.md`, `TODO.md`, `AUDIT.md`

Build: `update_handoff.sh`

Secrets: `Config.local.h` (never commit)

---

*If the docs and the code disagree, the code wins. Fix the docs.*
