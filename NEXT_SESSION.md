# Bike-Mate — Session Handoff for Next AI

You are picking up the Bike-Mate project. This file is the minimum
context to avoid 4 hours of re-asking. Read it fully before touching
code. Everything here is verified against the actual repo state.

---

## Project one-liner

Motorcycle battery / ride logger on ESP32-C3. BLE + OLED + NTC +
voltage divider + GPS + Python GUI. Prime directive: don't drain
the bike battery past the point where it won't crank.

---

## The human

- Retired engineer, Queensland Australia, Sprint ST 1050
- GUI runs 24/7 on the Mac, watches every bump on the graph
- OLED is off 99% of the time (bench tool, not field interface)
- Rides weekly minimum, idles the bike 5 min if not ridden
- Does not look at the dash while riding (crash risk)
- Communicates in short bursts, edits files himself, does not want
  you to write nine scripts at once
- Hates nano, loves one-line commands
- Shell is zsh on macOS Catalina — heredocs over ~10 lines break,
  '#' triggers history expansion, multi-line pastes fail constantly
- Calls cars "cages"

---

## Where the bike lives

- Carport, covered, QLD heat/humidity
- Router 4m, Mac 4m — WiFi path is reliable
- Bike is the ONLY thing on this project — no car, no other vehicle

---

## The battery (this is the important bit)

**SSB Powersport VTX12-BS** (replaces stock YT12B-BS)
- 12V 12Ah AGM
- Installed 2024-06-07, currently 2y 4m old
- Bought after the **previous battery died in 2 weeks with no warning**
- That is the actual problem Bike-Mate solves: "see it coming"

Voltage thresholds that matter:

| Voltage | Meaning |
|---|---|
| 12.8–13.0V | Fully charged |
| 12.6–12.7V | Good, will crank |
| 12.3–12.5V | Marginal |
| 12.0–12.2V | Will struggle to crank (crank floor) |
| < 12.0V | Won't crank |

Cranking current for the 1050 triple: ~150–200A inrush.

---

## What is actually installed and running

**Firmware:** V4.36 (tagged v4.36), running on the bench
**GUI:** v3.15
**Repo:** https://github.com/bigbadevilaussie-hue/Bike-Mate
**Local:** ~/Documents/Arduino/bike_mate/

**Bench setup:**
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, SDA=6, SCL=9)
- Mini 360 buck (12V->5V) — SUSPECT, 10mA idle, killing battery
- MP1584EN buck (5-pack) — arrived, not yet tested
- Bench PSU (30V/3A)
- USB-C powering the ESP32 during bench work
- PSU powering the 12V rail (divider + buzzer only)

**Not yet wired:**
- TVS diode (SA16CA)
- IRF4905 + PN2222A switched 12V rail (GPS + USB charger)
- GY-NEO6M GPS
- QC3.0 USB charger
- 12V rocker switch
- 12V piezo buzzer (as distinct from kit buzzer)

---

## Traced circuit (verified against the actual breadboard)

**Voltage divider (GPIO 0):**

    12V --[98.8k]--+--[9.98k]-- GND
                   +--[100nF]-- GND
                   +-- GPIO 0

Ratio: 9.90:1. Firmware assumes 10:1 -> ~1% high.

**NTC (GPIO 3):**

    3.3V --[MF52AT 10k B3950]--+--[9.7k]-- GND
                               +-- GPIO 3

No filter cap. Firmware still uses NTC_B 4600, NTC_SERIES 10000,
offset -4.8f. All three wrong for this part.

**OLED (I2C):**
GPIO 6 SDA, GPIO 9 SCL, 3.3V, addr 0x3C. GPIO9 is a boot strap pin.

**Buzzer (GPIO 4):**

    12V -- red wire -- 12V piezo -- black wire -- PN2222A collector
                                                    B --[1k]-- GPIO 4
                                                    +--[10k]-- GND
                                                    E -- GND

Piezo, so no flyback diode needed.

**ACC LED (GPIO 1):** green LED + 324ohm to GND.
**Status LED (GPIO 10):** red LED + 330ohm to GND.
wakeFlash() on every wake = double-blink = fake alarm.

---

## Board quirks (critical)

**XCW ESP32-C3 SuperMini has a known WiFi problem.**
Requires TX power reduced to 8.5 dBm:

    esp_wifi_set_max_tx_power(34);

Default TX (~20dBm) fails to connect. This is in WifiManager.cpp.
Took 4 AIs and 2 days to figure out. Do NOT suggest raising it.

**ESP32-C3 with 0.42" onboard OLED (2 in hand, unused):**
WiFi works at default TX. Candidate for future build.

**SCL is GPIO 9** — an ESP32-C3 boot strap pin. Do not add strong
pull-downs. OLED module's pull-ups hold it high at boot.

---

## Firmware state

**Current version:** V4.36
**Build partition:** PartitionScheme=min_spiffs (1.9MB APP / 190KB SPIFFS)
Binary is ~1.66MB, will NOT fit in default partition. IDE stores
partition per board type — switching between projects loses the setting.
Always use: arduino-cli compile --fqbn "esp32:esp32:esp32c3:PartitionScheme=min_spiffs"

**Settings module (V4.23+):**
- Settings.h / Settings.cpp — runtime-tunable values in NVS "bikeset"
- 3 tunables: runningEnter_mv, runningExit_mv, underRun_mv
- NVS keys: v.run.on(8) v.run.off(9) v.under(7) — all under 15 chars
- Zero from NVS = unset, defaults fill in
- Defaults live ONLY in set_defaults() — never in Config.h
- BLE characteristic SETTINGS_UUID = ...26b0

**Known-broken right now:**
1. macOS CoreBluetooth cache doesn't see ...26b0 on the GUI. Scan
   script sees it, GUI doesn't. Fix: sudo pkill bluetoothd then
   reboot GUI, or Mac reboot. Not a firmware problem.
2. settingsApplyJson mutates config.runningEnter_mv/etc BEFORE the
   cross-check reject. Rejected writes still change live behaviour
   until reboot. Fix: validate into locals, commit atomically.
3. GUI Apply path doesn't subscribe to the ACK notify — reports
   "Saved to NVS" based on write success only.

**Sprint 1 safety items STILL OPEN (do NOT tag anything until these
are closed):**
- BENCH_MODE 1 is still compiled in
- readSensors() early-return leaves stale voltage on ADC fault
- V_PANIC_ENTER 12.0 still below stated 12.2V crank floor

---

## Docs in the repo (all committed, all current)

| File | Purpose |
|---|---|
| README.md | Overview |
| PROJECT_STATE.md | Snapshot |
| FILES.md | File index |
| HANDOFF.md | Session handoff |
| BENCH.md | Physical breadboard, traced circuits, hardware TODO |
| AUDIT.md | 4-AI peer review (Claude, Gemini, ChatGPT, Mistral) |
| TODO.md | Ranked action list, A/B/C by verifiability |
| NEXT_SESSION.md | This file |

Rule: if the docs and the code disagree, the code wins. Fix the docs.

---

## What the last session ended on

- V4.36 committed, tagged, running on bench
- Settings dialog blocked by macOS BLE cache
- Partial-apply bug found by Le Mistral, not yet fixed
- Sprint 1 safety items skipped three versions in a row

**The pattern that failed:** three versions shipped (V4.23/24/25)
while Tier-1 safety items sat open. Adopt "no tagged release while
a Tier-1 blocker is open" going forward.

---

## What the next session should do, in order

**1. V4.26 firmware batch (one flash):**
   - Partial-apply fix in settingsApplyJson (validate into locals)
   - BENCH_MODE 0
   - readSensors() fault handling
   - V_PANIC_ENTER 12.0 -> 12.2
   - Optional: 0xFE vs 0xFF reject codes for range vs cross-check

**2. GUI Apply-ACK fix (no flash):**
   - Subscribe to SETTINGS_UUID once
   - Write, wait for 0x01 (OK) or 0xFF (fail)
   - Feed settings JSON into color bands (currently hardcoded 13.8/12.5/12.0)

**3. Clear BLE cache:** sudo pkill bluetoothd, reopen GUI

**4. Full settings round-trip test:** read -> change -> Apply -> reboot -> read

**5. THEN build report-mate.py:**
   - Reads ~/bike-mate-drive/wakes/*.csv and rides/*.csv
   - Plots voltage + temp over time
   - Corrected voltage trend (temperature-adjusted)
   - "Collecting baseline" for first 28 days
   - Flags buck drain vs aging

**6. THEN install on the bike** (MP1584EN swap, TVS, MOSFET, GPS)

**7. THEN 4 weeks of live data** for the first real diagnosis

---

## Communication rules (learned the hard way)

- Do NOT write 9 scripts at once. One file, one patch, one command.
- Do NOT guess anchors. Have the user paste the current file first.
- Do NOT paste multi-line scripts into zsh. Write to /tmp/foo.py first.
- Do NOT explain three things at once. Answer, then stop.
- The user edits files himself. Ask before assuming he wants automation.
- "Cages" = cars. "Sprint" = the bike, not a software sprint.
- Do NOT comment on time of day. No "it'''s late", "go to bed",
  "call it a night", or scheduling advice. Nick decides when
  to work and when to stop. This is not your call.

---

## The core insight

The prime directive isn't "protect the battery from itself." It's
"the rider already maintains the battery weekly — the device must
not silently undo that, and must show the trend before a cliff-edge
failure like the last battery."

The 10mA Mini 360 is the actual threat. Fix or replace it, or the
4-week baseline is meaningless.

---

## GUI 4.00 plan (started 2026-10-02)

Current: GUI v3.15, monolith `bikemate.py` (~1000 lines)
Target: GUI v4.00, modular `bikemate/` package + theme system + Fan-Mate-style reports

Order of work:
  1. Split `bikemate.py` into a `bikemate/` package (pure refactor, no behaviour change)
     - config.py, state.py, helpers.py, servers.py, drive.py,
       ble_worker.py, widgets.py, reports.py, dialogs.py, app.py
     - Keep a 6-line `bikemate.py` launcher importing from the package
  2. Port Fan-Mate's theme system (THEME_DAY / THEME_NIGHT, is_daytime(),
     current_theme(), apply_theme(), Toggle Day/Night menu item)
  3. Rewrite BikeReport in Fan-Mate style (ReportPlot widget, stat cards,
     warning banner, three stacked graphs)
  4. Bump GUI_VERSION 3.15 -> 4.00

Fan-Mate reference (for structure + theme):
  ~/Documents/Arduino/fanmate/fanmate/
    config.py     THEME_DAY, THEME_NIGHT, is_daytime helpers
    state.py      shared runtime state + fonts
    widgets.py    Card, Graph, ReportPlot classes
    reports.py    Report2H / ReportDaily / ReportWeekly
    app.py        App class + menu + toggle_theme + apply_theme
    helpers.py    time + status + version helpers

Decision deferred: monolith is fine for now. Split only if the file
grows past ~1500 lines or a section becomes genuinely hard to navigate.

---

## Session 2026-10-02 evening — GPS + OLED + theme

### Shipped (V4.46, committed)
- GUI 3.15 -> theme system ported from Fan-Mate (day/night, THEME_DAY/THEME_NIGHT)
- Toggle Day/Night menu item
- Reports submenu -> Last 2 Hours / Daily / Weekly
- Settings dialog + report window themed
- Firmware V4.42-4.46 in sequence:
  - V4.42: wake log pauses during rides (settle -> pause, arming -> resume)
  - V4.42: 5s ride log timer (was 30s, now 5s via separate main-loop timer)
  - V4.42: real state column in wake log (0=MONITOR, 1=RUNNING, 2=PANIC)
  - V4.42: TZ fix - setenv("TZ","AEST-10") at top of file builders
  - V4.42: RTC wake-overhead fix (totalSeconds += millis()/1000 before sleep)
  - V4.43: ADC mux throwaway read before NTC sampling
  - V4.43: settings + report theming (GUI)
  - V4.44: TZ fix confirmed working (Rule 7 now matches n==np)
  - V4.44: MAIL_ENABLED 0 (bench mode, no emails)
  - V4.45: GpsModule.cpp/.h written - NMEA parser, no external library
  - V4.45: GPS wired into bike_mate.ino (init + tick + stub removed)
  - V4.45: GPS_STUB_ENABLED 0
  - V4.46: GPS moved from GPIO 20/21 to GPIO 5/7 (WiFi conflict)
  - V4.46: OLED speed screen (big speed middle, volts/temp/time bottom)
  - V4.46: ride log lat/lon from GPS
  - V4.46: RTC persistence for GPS lat/lon/sats
  - V4.46: version bump, MAIL_ENABLED 0

### Known issues
- **GPS not reading on GPIO 5.** All `[GPS] fix=0 sats=0` even with wire on GPIO 5.
  Suspect: Serial1 pin mapping on C3, or wire not making contact.
  Next step: add avail= to init print, check RX buffer, DMM on GPS TX.
- **BLE HCI error spam.** `BT_HCI: CC evt: op=0x2008/0x2009 status=0x7`
  every 3s. Deferred to production board. Advertising restart fix pending.
- **Temp sawtooth on OLED/report.** Deferred to production board (better decoupling).
- **Dual upload.** Both `.csv` and `.csv.gz` upload to Drive. Rule 7 skips
  the raw but the `.gz` still uploads and deletes. Minor.
- **Rule 7 confirmation.** TZ fix worked, n==np match, newest ride stays.
  Verified in serial: `[UPLOAD] skipping newest ride ride_20261002210204.csv`.
- **NVS was blank** after flashing HW test sketch with erase. Rebuilds naturally.

### Hardware status
- GPS VCC -> 5V from buck (was 3.3V, brownout issue fixed)
- GPS TX -> GPIO 5 (was GPIO 20, WiFi interference)
- GPS RX -> GPIO 7 (unused)
- GPS GND -> GND
- OLED, NTC, voltage divider, LEDs, buzzer all verified working (HW test sketch)
- MOSFET switched rail on breadboard (verified Section 6 BENCH.md)
- No TVS, no fuse yet
- Mini 360 still the buck (12mA quiescent) - MP1584EN ordered, not arrived

### Pending for next session
1. Fix GPS reading on GPIO 5 (avail= diagnostic)
2. Walk test outdoors - get a fix, confirm lat/lon in ride log
3. Speed display verification (OLED screen)
4. GUI report rewrite (Fan-Mate style stat cards + banner)
5. GUI modularisation (still monolith, ~1100 lines)
6. GUI version bump 3.15 -> 4.00 (after report rewrite)
7. Rule 7 `.gz` skip (dual upload)
8. Pending seal cleanup (stale .sealed after failed upload)
9. Field install prep (MP1584EN swap, BENCH_MODE 0, TVS, fuse)

### Files changed this session
- Config.h (version, MAIL_ENABLED, GPS_STUB_ENABLED, BENCH_MODE)
- DriveUpload.cpp (Rule 7, TZ fix, Rule7 debug)
- WakeLogger.cpp (state write, TZ fix)
- Sensors.cpp (ADC mux settle)
- RideLogger.cpp (GPS lat/lon)
- GpsModule.cpp / GpsModule.h (new)
- bike_mate.ino (GPS init/tick, pause/resume, 5s ride timer, RTC fix)
- bikemate.py (theme system, reports submenu, settings theming)
- BENCH.md (Section 6, version consistency)
