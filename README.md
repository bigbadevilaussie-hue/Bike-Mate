# Bike-Mate

Motorcycle battery, temperature and ride logger. ESP32-C3 + BLE + OLED.

**Prime directive: Bike-Mate must not drain the bike battery past the point
where it will not crank the engine.** On the Sprint ST 1050 with a VTX12-BS
AGM, the practical cranking floor is ~12.2 V rested.

---

## Current state

- **Firmware:** see `Config.h` → `BIKE_MATE_VERSION`
- **GUI:** see `bikemate/config.py` → `GUI_VERSION`
- **Repo:** https://github.com/bigbadevilaussie-hue/Bike-Mate
- **Local:** `~/Documents/Arduino/bike_mate/`
- **Hardware:** breadboard (perfboard pending)
- **Bike:** Triumph Sprint ST 1050

---

## Hardware

### Bench
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, addr 0x3C)
- MP1584EN buck module (12 V → 5 V)
- 12 V bench PSU (30 V / 3 A)
- 12 V divider 98.8k / 9.98k on GPIO 0
- MF52AT NTC 10k (Beta 3950) on GPIO 3, with 100 nF ceramic
- PN2222A + IRF4905 switched 12 V rail (breadboard, LED sim)
- GY-NEO6M V2 NEO-6M GPS wired on bench

### Pin map
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

Note: GPIO 9 is an ESP32-C3 boot-strapping pin. Do not add strong external
pull-downs or large capacitors.

---

## What it does

- Wakes on a timer (30 s bench, 300/600 s field), reads voltage and temp,
  runs a state machine, logs a wake row.
- Detects engine start/stop by voltage (engine start >13.8 V, stop <13.0 V)
  and logs a full ride CSV while the engine is running.
- Advertises over BLE. GUI connects, reads live telemetry, pulls the newest
  ride summary and rows.
- Uploads wake and ride CSVs to Google Drive via Apps Script at 04:00 local
  (or on demand in bench mode). Deletes local file on success.
- Sends Gmail SMTP alerts on low battery and PANIC.
- Deep sleeps between wakes. Target sleep current is the whole point of the
  design — see the Prime directive.
- Maintenance mode (see below) brings WiFi up on demand for serial and OTA.

---

## Firmware

- **Framework:** Arduino IDE 2.x, ESP32 core **2.0.17** (3.x not supported)
- **Partition:** Minimal SPIFFS — the default partition is too small
- **Build:** `arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs" .`
- **Serial:** 115200
- **BENCH_MODE** in `Config.h` selects wake interval:
  - 1 = 30 s wakes (bench only — do not deploy)
  - 0 = 300 s day / 600 s night (field)

### Firmware modules
| File | Purpose |
|---|---|
| `bike_mate.ino` | Coordinator: setup, loop, state machine, sleep, maint consume |
| `Config.h` | Pins, UUIDs, thresholds, structs, feature flags |
| `Config.local.h` | Gitignored. WiFi/Gmail credentials |
| `Sensors.cpp` | ADC reads for voltage and thermistor |
| `Buzzer.cpp` | Alarm + low-batt beep sequences |
| `RideLogger.cpp` | Ride lifecycle, summary to NVS |
| `RideStorage.cpp` | LittleFS ride CSV writer |
| `WakeLogger.cpp` | LittleFS wake CSV writer (date-keyed) |
| `BleManager.cpp` | BLE server, telemetry push, maint trigger, settings |
| `DisplayManager.cpp` | OLED rendering, edge-triggered updates |
| `WifiManager.cpp` | WiFi bring-up, clock fetch (Opal LAN + NTP fallback) |
| `WifiMail.cpp` | Gmail SMTP alerts |
| `DriveUpload.cpp` | Wake + ride CSV upload to Drive via Apps Script |
| `OtaManager.cpp` | Legacy BLE-URL OTA. Retired, still compiled |
| `Settings.cpp` | Runtime settings in NVS `bikeset` |
| `GpsModule.cpp` | NMEA parser for NEO-6M, no external library |
| `SerialBuffer.cpp` | Ring buffer feeding /serial-raw |
| `WebServer.cpp` | HTTP server for maintenance mode |

### HTTP endpoints (live during maint only)
| Route | Method | Purpose |
|---|---|---|
| `/serial` | GET | HTML serial page (live tail) |
| `/serial-raw` | GET | Plain text ring buffer |
| `/maint/off` | POST | Exit maintenance mode |
| `/ota` | POST | Firmware upload (multipart) |
| `/ota-progress` | GET | OTA progress JSON |
| `/version` | GET | `BIKE_MATE_VERSION` as text |
| `/settings` | GET | Settings JSON |
| `/settings` | POST | Apply settings JSON |

---

## Maintenance mode

The bike sleeps most of the time. To do anything interactive — read serial,
flash firmware, change settings — the bike must be woken into **maintenance
mode**: WiFi up, HTTP server running, BLE off, awake for a bounded window.

- **Enter:** GUI menu → Maintenance → Activate. GUI writes `{"maint":"on"}`
  to the bike over BLE. Firmware sets `maintRequest = true` (RTC flag),
  ACKs, sleeps. On the next wake, `bleInit()` is skipped and WiFi comes up.
- **Exit:** `/maint/off` from the serial page or GUI, engine start detected
  (voltage crosses `runningEnter_mv`), or the 15-min timeout.
- **Safety cap:** `MAINT_MAX_MS` (15 min) in `Config.h`.
- **During maint:** BLE off, WiFi up, serial page live, OTA and settings
  available over HTTP. OLED shows `MAINT / MODE / <wifi state> / Ns left`.
- **Clock:** Opal LAN (GL-SFT1200) is primary — fetch the Date header from
  `http://<gateway>/`. NTP is fallback. No hard gate.
- **WiFi:** static IP `192.168.8.196`, gateway `192.168.8.1`, reserved on
  the Opal. DHCP was unreliable and is disabled.
- **TX power:** 8.5 dBm (`esp_wifi_set_max_tx_power(34)`). Mandatory on
  this board — default TX breaks association.
- **Coexistence:** the C3 in core 2.0.17 fails WiFi association after BLE
  sometimes. `wifiBringUp()` retries 2× with a 3 s settle. `bleStop()` waits
  for the BT controller to reach IDLE before WiFi init.

OTA is HTTP only. GUI POSTs the compiled `.bin` to `/ota?ver=X.YZ`.
Firmware streams into `Update.h` and reboots. The old BLE-URL OTA path
(`OtaManager.cpp`) is retired.

---

## GUI

Python/Tk, modular package:

    bikemate/
      __init__.py
      config.py    — constants, themes, UUIDs, IPs, timeouts
      state.py     — mutable globals, deques, locks
      helpers.py   — utility funcs, time/emoji helpers
      weather.py   — weather fetch + background loop
      widgets.py   — Graph
      ota.py       — HTTP OTA servers
      drive.py     — Drive sync + backup + local server
      ble.py       — BLEWorker
      reports.py   — BikeReport, LastRideReport, WaitingForRide
      app.py       — App class (menu, tick, main window)
    bikemate.py    — 10-line launcher

Edit modules in `bikemate/`, not the launcher.

**Requires:** `bleak`, `requests`, Python 3.8+. Tk is stdlib.

**Features:**
- Live telemetry over BLE (v, t, acc, engine, warn, state)
- Ride pull over BLE (STREAM + REQUEST characteristics)
- Maintenance mode control (activate/deactivate, state-aware menu)
- Serial page (HTTP, during maint)
- OTA over HTTP with progress stage polling
- Settings read/write over HTTP during maint
- Drive sync (pull from Apps Script)
- Two local HTTP servers on ports 8000 (OTA staging) + 8001 (Drive mirror)

---

## Rules

Non-negotiable. Read first.

### Tone

- Be direct. No lectures. No explaining basic git concepts.
- Talk like a competent senior dev, not a tutorial.
- If you're about to explain something obvious, shut up and give the command.
- No time-of-day references. No sleep/rest/break suggestions.
- Nick decides when to work and when to stop.

### Commits

- "fully commit" / "complete the commit" = `git add -A`, commit, push to
  origin, tag if a version was mentioned.
- Never say "working tree clean" unless asked for status.
- Never ask to re-check output already pasted.

### Shell — zsh

- Multi-line pastes break. `#` triggers history expansion.
- **Write patches as Python scripts to `/tmp/` and run with `python3
  /tmp/patch.py`.** Never paste heredocs directly into the shell.
- Every patch script must exit non-zero on anchor miss. Silent no-ops are
  worse than crashes.
- Bike-Mate compile requires `:PartitionScheme=min_spiffs`.

### Patch discipline

- **Never patch a file you haven't seen the current contents of.** If the
  last paste was more than one exchange ago, ask again.
- **Never guess anchors or regex.** Read the file. Grep for the exact line.
  If the anchor misses, stop and ask for current contents. Do not guess.
- **One patch. One file when possible. Test before the next.**
- **Version bump is part of every patch.** `[BOOT] BIKE-MATE VX.YZ` is the
  ground truth for which binary is on the board. Never trust a paste that
  disagrees with the log.
- **After every patch, state how to verify it worked.** A specific log
  line, OLED state, or endpoint response. "Compile and flash" is not
  verification.

### Diagnosing

- **Never patch a theory.** Propose it, propose the cheapest way to
  confirm, get the answer, then patch.
- **Git history is the answer more often than the code.** When something
  regressed, `git log --oneline -- <file>` and `git log -L` show when.
- **If a subsystem has a comment like "proven 72/72" or "two days and four
  AIs", do not touch it on a theory.** Ask why it's that shape first.
- **Intermittent ≠ broken.** The fix is usually a retry, not a rewrite.

### Communication

- Answer, then stop. Do not explain three things at once.
- Do not write nine scripts at once. One file, one patch, one command.
- Nick edits files himself. Ask before assuming automation.
- "Cages" = cars.

### Session hygiene

- This is one stateful session, not isolated replies.
- Before proposing a fix, ask: "have I confirmed current state, or am I
  assuming it?"
- Before saying "this is a bug", ask: "could this be load-bearing for a
  reason I don't see?"
- Read all pasted output. The answer is usually in the line you skimmed.

### Bike-Mate specific

- 8.5 dBm TX power is mandatory on the XCW Super Mini.
- Static IP 192.168.8.196, gateway 192.168.8.1.
- BLE off during maint. Do not try to use both at once.
- Opal LAN clock is primary. NTP is fallback. Never hard-gate on NTP.
- Opal sometimes takes 3 s to serve `/`. That is the Opal, not the firmware.
- `Config.local.h` is gitignored. Never commit it, never paste a dump
  containing it.
- Reports module (`bikemate/reports.py`) is functional but basic. No KPI
  board, no cross-ride trends. That is planned work, not a bug.
