# Bike-Mate

**Handoff doc for the next AI. Not for humans.** Nick doesn't read
this. Keep it accurate so the next session starts clean.

Motorcycle battery, temperature and ride logger. ESP32-C3 + BLE + OLED.

**Prime directive: Bike-Mate must not drain the bike battery past the
point where it will not crank the engine.** On the Sprint ST 1050 with
a VTX12-BS AGM, the practical cranking floor is ~12.2 V rested.

---

## Current state

- **Firmware source:** `Config.h` -> `BIKE_MATE_VERSION` = **6.00**
- **Firmware on bike:** **V6.00** (bench testing, 2026-11-01)
- **GUI:** `bikemate/config.py` -> `GUI_VERSION` = **4.58**
- **OSA:** `osa_server.py` -> `__version__` = **1.33** (separate repo,
  runs 24/7 on the Mac)
- **Repo:** https://github.com/bigbadevilaussie-hue/Bike-Mate
- **OSA repo:** https://github.com/bigbadevilaussie-hue/OSA-Server
- **Logs repo:** https://github.com/bigbadevilaussie-hue/Bike-Mate-Uploads
- **Local:** `~/Documents/Arduino/bike_mate/`
- **Bike:** Triumph Sprint ST 1050 (production veroboard build)

### Hardware status

The production veroboard board is built and on the bench:
- All components soldered, tracks cut, and tested.
- Voltage and temperature calibrated.
- Sleep current: 1 mA. Awake (running): 77 mA.
- MOSFET switch verified (turns GPS/USB off in sleep).
- Ready for 7-day field test on the bike.

### Upcoming Tasks (Pre-Field Test)

- Fix all OLED screens for the 128x32 display (some layouts are still clipping).
- Remove the stall function (`FLAG_STALL`) from the ride logger.
- Perform the 7-day field test.

---

## Hardware

- ESP32-C3 SuperMini + 0.96" SSD1306 OLED (I2C, 0x3C)
- MP1584EN 12->5 V buck, 12 V bench PSU
- Divider 98.8k/9.98k on GPIO 0, MF52AT NTC 10k on GPIO 3
- NEO-6M GPS (RX/TX GPIO 2/21)
- Push button on GPIO 5 (wake / Rider for Rider)
- Piezo buzzer on GPIO 4 (bit-bang 2.8 kHz, no LEDC)

**Pin map:** 0=voltage, 1=ACC LED, 3=thermistor, 4=buzzer, 5=wake
button, 6=SDA, 9=SCL, 10=status LED.

GPIO 9 is a boot-strapping pin - no strong pull-downs or caps.

---

## What it does

- Timer wake (30 s bench, uniform 5 min field) -> read V/T -> state
  machine -> log row.
- Engine start >13.8 V, stop <13.0 V. Ride CSV while engine is running.
- BLE telemetry (26a8) consumed by **OSA, not the GUI**. The GUI polls
  OSA's `/bike/status` over HTTP.
- **Auto upload path (BLE -> OSA).** On the 04:00 schedule, or on a
  manual `{"cmd":"upload"}` written to 26ae, the bike seals + gzips
  its wake and ride files, then ships each over **26b1** in 180-byte
  chunks. OSA reassembles and writes to `~/bike-mate-drive/` on the
  Mac. GitHub push by OSA: **# TODO: verify**.
- **Manual upload path (WiFi -> GitHub).** `POST /seal` + `POST
  /upload` during maint. Same GitHub Contents API destination, over
  WiFi.
- Every file gzipped; raw deleted after gzip. Newest ride held back
  (Rule 7) for the GUI's pull path.
- Gmail SMTP alerts on low battery / PANIC.
- **Reader for Rider:** button press wakes the bike for 120 s,
  OLED shows RIDER / READY countdown. Field mode only.
- **Stall detection:** short ride (< 60 s engine run) flagged
  `FLAG_STALL` in the ride summary, logged in wake file.
- **Cool Down Mode:** after a real ride and arming, 120 s awake with
  1 Hz wake-log rows (`state=5`). BLE off, OLED off, LEDs off,
  buzzer silent. Engine restart ignored.
- Deep sleeps between wakes. Prime directive: never drain the battery.
- Maint mode brings WiFi up on demand for serial + OTA.

---

## Firmware

- Arduino IDE 2.x, ESP32 core **2.0.17** (3.x unsupported)
- Partition: **Minimal SPIFFS**
- Build: `arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs,CDCOnBoot=cdc" .`
- Serial: 115200
- `benchMode` in NVS: 1 = 30 s wakes (bench), 0 = 5 min (field)

### Firmware modules

| File | Purpose |
|---|---|
| `bike_mate.ino` | Coordinator: setup, loop, state machine, sleep, maint consume, Cool Down Mode |
| `Config.h` | Pins, UUIDs, thresholds, structs, feature flags |
| `Config.local.h` | Gitignored. WiFi/Gmail credentials |
| `Sensors.cpp` | ADC reads for voltage and thermistor |
| `Buzzer.cpp` | Bit-bang 2.8 kHz chirp pairs (no LEDC) |
| `RideLogger.cpp` | Ride lifecycle, summary to NVS |
| `RideStorage.cpp` | LittleFS ride CSV writer |
| `WakeLogger.cpp` | LittleFS wake CSV writer (date-keyed) |
| `BleManager.cpp` | BLE server, telemetry push, maint trigger, settings, `bleSendFile` over 26b1 |
| `DisplayManager.cpp` | OLED rendering, edge-triggered updates |
| `WifiManager.cpp` | WiFi bring-up, clock fetch (Opal LAN + NTP fallback) |
| `WifiMail.cpp` | Gmail SMTP alerts |
| `DriveUpload.cpp` | WiFi-path upload to GitHub Contents API; `reconcileLittleFS()` exported for the BLE path |
| `OtaManager.cpp` | Legacy BLE-URL OTA path. Retired for OTA, but `OtaCallbacks` in BleManager still parses the OTA JSON envelope |
| `Settings.cpp` | Runtime settings in NVS `bikeset` |
| `GpsModule.cpp` | NMEA parser for NEO-6M, no external library |
| `SerialBuffer.cpp` | Ring buffer feeding `/serial-raw` |
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
| `/upload` | POST | Force WiFi upload now (maint only, returns immediately) |
| `/seal` | POST | Manually seal the current wake file (and ride if logging) |
| `/clock` | POST | Set clock (decimal epoch string or 4 raw LE bytes) |

---

## Maintenance mode

Bike must be woken into maint for interactive work: WiFi up, HTTP
server, BLE off, bounded window.

- **Enter:** GUI -> Maintenance -> Activate. OSA queues
  `{"maint":"on"}` and writes it on the next bike connect.
- **Exit:** `/maint/off`, engine start, or `MAINT_MAX_MS` (5 min).
- **Clock:** Opal LAN Date header (`http://<gateway>/`), NTP
  fallback. Overridable via `POST /clock`. Never hard-gate on NTP.
- **WiFi:** static IP `192.168.8.196`, gateway `192.168.8.1`.
- **TX:** 8.5 dBm (`esp_wifi_set_max_tx_power(34)`) - mandatory.
- **Coex:** C3 core 2.0.17 fails WiFi after BLE sometimes.
  `wifiBringUp()` retries 2x with a 3 s settle. `wifiPingTest()`
  retries up to 4x with 3 s gaps (V5.55).

OTA is HTTP only. GUI POSTs `.bin` to `/ota?ver=X.YZ`.

---

## GUI

Python/Tk package `bikemate/`: `config`, `state`, `helpers`, `weather`,
`widgets`, `ota`, `drive`, `ble`, `reports`, `app`. Launcher is
`bikemate.py`. Edit modules, not the launcher.

Requires: `bleak`, `requests`, Python 3.8+, Tk, customtkinter.

Features: HTTP poll of OSA `/bike/status` (no BLE client), maint
control via OSA queue, serial page, HTTP OTA with progress, settings,
storage badge, Drive sync, local servers 8000 (OTA) + 8001 (Drive).

GUI 4.50 committed as `6a6639c`. Changelog:
- 4.47 — slide sidebar
- 4.48 — hide sleep schedule block in Settings
- 4.49 — day-only theme; settings dialog fixes (KeyError on hidden
  fields, transient+grab, Apply closes); top title restored; sidebar
  polish; Maintenance items grey out per state
- 4.50 — in-GUI serial popup polling /serial-raw

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

### Shell - zsh

- Multi-line pastes break. `#` triggers history expansion.
- **Write patches as Python scripts to `/tmp/` and run with `python3
  /tmp/patch.py`.** Never paste heredocs directly into the shell.
- Every patch script must exit non-zero on anchor miss. Silent no-ops are
  worse than crashes.
- Bike-Mate compile requires `:PartitionScheme=min_spiffs`.
- zsh bracketed-paste mangles multi-line pastes even inside heredocs - lines
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
- **Intermittent != broken.** The fix is usually a retry, not a rewrite.

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

---

## Refactor in progress (V5.58+)

Migration to "OSA is the hub". **OSA side is done (v1.17 + v1.18).**
Firmware and GUI sides still pending. This section is the plan for
the next session.

**Target architecture:**

    Bike (BLE 26b1) → OSA → GitHub (Bike-Mate-Uploads)
                       ↓
                      GUI (HTTP poll)

- Bike ships wake/ride/dyna/bin files over BLE to OSA. Already works.
- OSA writes them to `~/bike-mate-drive/{wakes,rides,dyna,bin}/`.
  Already works.
- **OSA pushes to `Bike-Mate-Uploads` on a 2-hour timer.** Not built.
- **GUI reports read from OSA over HTTP** (`GET /bike/files`).
  Not built.
- The WiFi → GitHub upload path from the bike retires. `DriveUpload.cpp`
  becomes a stub or gets deleted.
- The GUI's three orphan Drive functions (`open_drive`,
  `open_local_drive`, `menu_sync`) get removed.

**5 steps, in order:**

1. **OSA v1.17 — GitHub pusher thread (2h timer)** — ✅ shipped
   (verified live: 177 commits in Bike-Mate-Uploads, runs at :09
   past odd hours, pushes wakes/rides/dyna/bin)
2. **OSA v1.18 — `GET /bike/files` for the GUI's reports** — ✅
   shipped (used by `bikemate/reports.py::_osa_list` and `_parse`)
3. Firmware V5.58 — retire `DriveUpload.cpp`, route manual upload
   through BLE
4. GUI 4.51 — reports read from OSA, delete Drive code
5. Docs — this README and the OSA README, after all four land

Each step gets its own patch → compile → verify cycle.

**Decisions pending (Q1-Q7):**

| # | Question | Proposed default |
|---|---|---|
| Q1 | `reconcileLittleFS()` caller after retirement | every wake |
| Q2 | `GH_TOKEN` source for OSA | read `Config.local.h` |
| Q3 | 2h timer semantics | push at startup + every 2h |
| Q4 | Manifest location | one `.pushed.json` at `~/bike-mate-drive/` top |
| Q5 | 422 (file exists on GitHub) | mark as pushed |
| Q6 | `driveUploadPerform` | retire entirely |
| Q7 | GitHub folder case | uppercase (`BIN/`) matching firmware |

Answers needed before step 1 starts. Next AI: ask Nick for these if
they aren't answered in the session.

**What does NOT change:**

- The 04:00 scheduled auto-upload trigger on the bike (only the
  transport changes, WiFi → BLE)
- `bleSendFile()` in `BleManager.cpp`
- `wakeLoggerSealManual()`
- `/seal` endpoint
- OSA's BLE central thread
- OSA's `_bike_on_upload` reassembly + local write

---

## Rules (amended)

Same rules as above, plus:

- **zsh `?` glob trap.** A `?` in an unquoted command line (e.g. in a
  `#` comment on the same line as a `grep`) triggers filename globbing
  and aborts the whole command with `no matches found`. Fix: quote the
  argument, or drop the comment, or `setopt +o nomatch`. Cost a
  debugging cycle on 2026-10-14.

- **Multi-line shell blocks via `cat >> file <<'EOF'`** work in zsh
  if the heredoc tag is single-quoted. Never use `<<EOF` unquoted.

---

## # TODO: verify

- Fan-Mate HB black box — claimed by Nick as running on OSA, but
  `osa_server.py` shows `HB_LOG_DIR` hardcoded to `~/bike-mate-hb/`
  and `_hb_append` is only called from `_bike_on_notify`. Next AI:
  confirm whether Fan-Mate HB is planned, in a different file, or
  merged into the bike-mate HB log.
- ~~GUI 4.47/4.48 commit state~~ — resolved. 4.50 committed as
  `6a6639c`.

---

## Hardware build

`veroboard.txt` in this repo contains the physical build:

- Vero board dimensions (13 strips × 45 holes, 35 × 115 mm)
- Component placement plan by region
- Power rail routing (12V, 5V always-on, 5V switched, 3V3, GND)
- Cut list (where to sever copper tracks)
- Jumper list (wire links between strips)
- Full netlist (every electrical connection)
- Bill of materials
- Front panel / side / back panel cutout dimensions
- Pre-power-up checklist
- Power budget (always-on vs switched rails)

Hardware architecture in brief:

    AGM 12V
      │
      SAE → Kill switch → Fuse → TVS
                                  │
              ┌───────────────────┼───────────────────┐
              │                   │                   │
         MP1584EN             98.8k/9.98k        IRF4905 SOURCE
         12V→5V               divider                 │
              │              (GPIO 0)          IRF4905 DRAIN
             5V                                    │
              │                             Switched 12V
         ESP32-C3                                  │
         (always on)                    ┌──────────┼──────────┐
                                        │          │          │
                                    Mini 360   USB charger   (future)
                                    12V→5V
                                        │
                                    GPS + Fan

Prime directive: the always-on rail draws ~250 µA total
(MP1584EN quiescent + divider + ESP32 light sleep). On a
10.5 Ah AGM that's ~2.6% per month. Everything else —
GPS, fan, USB charger — lives on the IRF4905-switched rail,
gated by `accState`. The ESP32 switches it on when it
detects engine running (voltage > 13.8 V).

**Not yet built.** The doc is the spec; the board doesn't
exist yet.
