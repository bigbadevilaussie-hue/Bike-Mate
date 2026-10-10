# Bike-Mate

Motorcycle battery, temperature and ride logger. ESP32-C3 + BLE + OLED.

**Prime directive: Bike-Mate must not drain the bike battery past the point
where it will not crank the engine.** On the Sprint ST 1050 with a VTX12-BS
AGM, the practical cranking floor is ~12.2 V rested.

---

## Current state

- **Firmware:** `Config.h` → `BIKE_MATE_VERSION`
- **GUI:** `bikemate/config.py` → `GUI_VERSION`
- **Repo:** https://github.com/bigbadevilaussie-hue/Bike-Mate
- **Local:** `~/Documents/Arduino/bike_mate/`
- **Bike:** Triumph Sprint ST 1050 (breadboard build)

---

## Hardware

- ESP32-C3 SuperMini + 0.96" SSD1306 OLED (I2C, 0x3C)
- MP1584EN 12→5 V buck, 12 V bench PSU
- Divider 98.8k/9.98k on GPIO 0, MF52AT NTC 10k on GPIO 3
- NEO-6M GPS (RX/TX GPIO 2/21)

**Pin map:** 0=voltage, 1=ACC LED, 3=thermistor, 4=buzzer, 6=SDA, 9=SCL,
10=status LED.

GPIO 9 is a boot-strapping pin — no strong pull-downs or caps.

---

## What it does

- Timer wake (30 s bench, 300/600 s field) → read V/T → state machine → log row.
- Engine start >13.8 V, stop <13.0 V. Ride CSV while engine is running.
- BLE telemetry + newest-ride pull for the GUI.
- Uploads wake + ride logs to GitHub at 04:00 local, or on-demand via
  `/upload` during maint. Every file gzipped; raw deleted after gzip.
  Newest ride held back (Rule 7) for GUI pull over BLE.
- Gmail SMTP alerts on low battery / PANIC.
- Deep sleeps between wakes. Prime directive: never drain the battery.
- Maint mode brings WiFi up on demand for serial + OTA.

---

## Firmware

- Arduino IDE 2.x, ESP32 core **2.0.17** (3.x unsupported)
- Partition: **Minimal SPIFFS**
- Build: `arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs" .`
- Serial: 115200
- `BENCH_MODE` in `Config.h`: 1 = 30 s wakes (bench), 0 = 300/600 s (field)

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
| `DriveUpload.cpp` | Wake + ride CSV upload to GitHub via Contents API |
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
| `/upload` | POST | Force upload now (maint only, returns immediately) |
| `/clock` | POST | Set clock (decimal epoch string or 4 raw LE bytes) |

---

## Maintenance mode

Bike must be woken into maint for interactive work: WiFi up, HTTP server,
BLE off, bounded window.

- **Enter:** GUI → Maintenance → Activate. BLE writes `{"maint":"on"}`.
- **Exit:** `/maint/off`, engine start, or `MAINT_MAX_MS` (5 min).
- **Clock:** Opal LAN Date header (`http://<gateway>/`), NTP fallback.
  Overridable via `POST /clock`. Never hard-gate on NTP.
- **WiFi:** static IP `192.168.8.196`, gateway `192.168.8.1`.
- **TX:** 8.5 dBm (`esp_wifi_set_max_tx_power(34)`) — mandatory.
- **Coex:** C3 core 2.0.17 fails WiFi after BLE sometimes.
  `wifiBringUp()` retries 2× with a 3 s settle.

OTA is HTTP only. GUI POSTs `.bin` to `/ota?ver=X.YZ`. Old BLE-URL path
retired.

---

## GUI

Python/Tk package `bikemate/`: `config`, `state`, `helpers`, `weather`,
`widgets`, `ota`, `drive`, `ble`, `reports`, `app`. Launcher is
`bikemate.py`. Edit modules, not the launcher.

Requires: `bleak`, `requests`, Python 3.8+, Tk.

Features: BLE telemetry + ride pull, maint control, serial page, HTTP OTA
with progress, settings, storage badge, Drive sync, local servers 8000
(OTA) + 8001 (Drive).

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
- zsh bracketed-paste mangles multi-line pastes even inside heredocs — lines
  at the boundary get eaten silently. Fix once:
  `echo 'unset zle_bracketed_paste' >> ~/.zshrc` then `exec zsh`. Or run
  `bash` for the session.
- If a heredoc still misbehaves: write the script with
  `printf '%s\n' \` and one `'line' \` per line, redirect to `/tmp/x.py`,
  then `python3 /tmp/x.py`.
- Never paste Python with `python3 -c`. Always `/tmp/x.py`.
- A Python string anchor matches the exact bytes. If a file ends without a
  trailing newline, `"...;\n"` will MISS. Check with
  `python3 -c 'print(repr(open("f").read()[-40:]))'` before writing the anchor.

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
