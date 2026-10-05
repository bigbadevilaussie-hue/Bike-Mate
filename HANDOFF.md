# Bike-Mate — Chat Handoff

Generated: 2026-10-05

The rules below are non-negotiable. Read them first.

---

## RULES (non-negotiable)

- Be direct. No lectures. No explaining basic git concepts I already know.
- When I say "fully commit" or "complete the commit" I mean: `git add -A`,
  commit, push to origin, and tag if I mentioned a version.
- Never ask me to re-check things I already pasted output for.
- Never say "working tree clean" unless I specifically ask for status.
- If you're about to explain something obvious, shut up and just give the
  command.
- Talk like a competent senior dev, not a tutorial.

### Working with Nick's shell

- zsh. Multi-line pastes and long heredocs break. `#` triggers history
  expansion.
- Write patches as Python scripts to `/tmp/` and run with `python3
  /tmp/patch.py`. Never paste heredocs directly.
- Read the file before patching. Never guess anchors or regex.
- One patch at a time. Stop after each, test, next.
- Version bump is part of every patch.
- Bike-Mate compile requires
  `:PartitionScheme=min_spiffs` — default partition is too small.

### Behaviour

- No time-of-day references. No "it's late", no "go to bed".
- No sleep/rest/break suggestions.
- Nick decides when to work and when to stop.

---

## Current state

- **Firmware:** see `Config.h` → `BIKE_MATE_VERSION`
- **GUI:** see `bikemate.py` → `GUI_VERSION`
- **Repo:** https://github.com/bigbadevilaussie-hue/Bike-Mate
- **Local:** `~/Documents/Arduino/bike_mate/`

### Hardware
- XCW ESP32-C3 SuperMini
- External 0.96" SSD1306 128x64 OLED (SDA 6, SCL 9)
- MF52AT NTC 10k on GPIO 3
- Voltage divider on GPIO 0
- GPS not wired
- MOSFET ACC rail breadboard only

### Build
- Arduino IDE 2.x, ESP32 core 2.0.17
- Partition: Minimal SPIFFS (1.9MB APP / 190KB SPIFFS)
- `arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs" .`

---

## Architecture at a glance

### Normal operation (maint OFF)

- Bike sleeps on a timer (30 s bench, 300/600 s field).
- Wakes, reads sensors, runs state machine, logs a wake row.
- BLE advertises. GUI connects if present, pulls telemetry and any new ride.
- WiFi off.
- Upload fires on schedule (04:00 local, or every arming in bench mode).

### Maintenance mode (maint ON)

- Triggered by BLE `{"maint":"on"}` from the GUI.
- Firmware sets `maintRequest = true` (RTC flag), ACKs, sleeps.
- On next wake, `doStateWork` sees the flag, brings WiFi up (which stops
  BLE), starts an HTTP server, enters maint for up to `MAINT_MAX_MS`
  (15 min).
- During maint: BLE off, WiFi up, serial page live, OTA available via
  HTTP POST.
- Exit: HTTP `/maint/off`, or 15-min timeout.
- See `MAINTENANCE.md` for full detail.

### HTTP endpoints (only during maint)

| Route | Method | Purpose |
|---|---|---|
| `/serial` | GET | HTML serial page |
| `/serial-raw` | GET | Text serial dump |
| `/maint/off` | POST | Exit maint |
| `/ota` | POST | Firmware upload |
| `/version` | GET | Current firmware version |

### OTA

OTA is HTTP, only available in maint mode. GUI POSTs the compiled `.bin`
to `/ota`. Firmware streams it into `Update.h` and reboots.

The old BLE-URL OTA path is retired. `OtaManager.cpp` is legacy.

---

## Protected systems (do not casually modify)

- **OLED** — pins (6, 9), library, init, timing
- **Sleep/wake** — wake intervals, RTC state, sleep gate conditions
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify protocol
- **WiFi bring-up** — TX power must stay 8.5 dBm on this board
- **Upload pipeline** — 302 handling, URL-encoded POST format
- **Maintenance mode** — flag pattern (BLE set, next-wake consume),
  `MAINT_MAX_MS` cap, server lifecycle

Any change to these must be tested end-to-end on real hardware.

---

## What's next

See `TODO.md` for the full ranked list. Short version:

1. Set `BENCH_MODE 0` before deployment
2. Fix `readSensors()` early-return
3. Raise panic threshold to 12.2V
4. Measure sleep current (MP1584EN swap if needed)
5. Wire GPS, test outdoors
6. Complete the switched 12V rail
7. Field install + 4-week baseline

---

## Known open issues

- `BENCH_MODE 1` compiled in for dev — do not deploy
- `readSensors()` early-return leaves stale voltage on fault
- `V_PANIC_ENTER 12.0` below stated crank floor 12.2V
- MP1584EN quiescent current unmeasured
- GPS not wired
- WiFi NTP is a hard gate on maint entry — if NTP fails, maint fails
- `_mailFailCount` has no decay
- 302 handled as success without body check

---

## Docs index

- `README.md` — overview
- `PROJECT_STATE.md` — current snapshot
- `FILES.md` — file index
- `HANDOFF.md` — this file
- `MAINTENANCE.md` — maint mode architecture
- `BENCH.md` — physical bench, wiring, hardware TODO
- `TODO.md` — ranked action list
- `AUDIT.md` — historical 4-AI review (superseded)

If a doc and the code disagree, the code wins. Fix the doc.

---

## Communication rules (learned the hard way)

- Do not write nine scripts at once. One file, one patch, one command.
- Do not guess anchors. Have the user paste the current file first.
- Do not paste multi-line scripts into zsh. Write to `/tmp/foo.py` first.
- Do not explain three things at once. Answer, then stop.
- The user edits files himself. Ask before assuming he wants automation.
- "Cages" = cars.
- Do not comment on time of day.

---

## Prime directive

Bike-Mate must not drain the bike battery past the point where it will
not crank the engine. On the Sprint ST 1050 with a VTX12-BS AGM, the
practical cranking floor is ~12.2V rested.

The device must not silently undo weekly maintenance, and must show the
trend before a cliff-edge failure.

The Mini 360 buck was the actual threat at ~10 mA quiescent. MP1584EN
replacement pending measurement.
