# Bike-Mate — Chat Handoff

Generated: 2026-10-06

The rules below are non-negotiable. Read them first.

---

## RULES (non-negotiable)

### Tone

- Be direct. No lectures. No explaining basic git concepts I already know.
- Talk like a competent senior dev, not a tutorial.
- If you're about to explain something obvious, shut up and just give the
  command.
- No time-of-day references. No "it's late", no "go to bed".
- No sleep/rest/break suggestions.
- Nick decides when to work and when to stop.

### Commit semantics

- "fully commit" / "complete the commit" = `git add -A`, commit, push to
  origin, tag if I mentioned a version.
- Never say "working tree clean" unless I ask for status.
- Never ask me to re-check output I already pasted.

### Shell — zsh, and the paste rules exist for a reason

- Multi-line pastes break. `#` triggers history expansion.
- **Write patches as Python scripts to `/tmp/` and run with `python3
  /tmp/patch.py`.** Never paste heredocs directly into the shell.
- Every patch script must exit non-zero on anchor miss. Silent no-ops are
  worse than crashes.
- Bike-Mate compile requires
  `:PartitionScheme=min_spiffs` — the default partition is too small.

### Patch discipline — this is where every session loses time

- **Never patch a file you haven't seen the current contents of.** The
  version in `bike.txt` or an earlier paste may be several commits stale.
  If the last paste was more than one exchange ago, ask for the file again.
- **Never guess anchors or regex.** Read the file. Grep for the exact line.
  If the anchor misses, that is information — the file changed. Stop and
  ask for the current contents. Do not guess a looser anchor.
- **One patch. One file when possible. Test before the next patch.**
  Chaining three unverified patches means when the fourth fails you don't
  know which of the first three broke.
- **Version bump is part of every patch.** The version tells you which
  binary is on the board. `[BOOT] BIKE-MATE V4.XX` is the ground truth.
  Never trust a paste that says one version when the log says another.
- **After every patch, state how to verify it worked.** A specific log
  line, a specific OLED state, a specific endpoint response. "Compile and
  flash" is not verification.

### Diagnosing — the biggest time sink

- **Never patch a theory.** Propose it, propose the cheapest way to
  confirm or refute it, get the answer, then patch.
- **The git history is the answer more often than the code is.** When
  something regressed, `git log --oneline -- <file>` and `git log -L` show
  when. Read that before reasoning about what "should" have changed.
- **If a subsystem has a comment like "proven 72/72" or "two days and four
  AIs", do not touch it on a theory.** The comment is a warning that the
  current shape is load-bearing. Ask why it's that shape first.
- **Intermittent ≠ broken.** If a subsystem works sometimes, the fix is
  usually a retry or a pre-condition, not a rewrite. Rewriting working-
  sometimes code loses the cases that were working.

### Communication

- Answer, then stop. Do not explain three things at once.
- Do not write nine scripts at once. One file, one patch, one command.
- The user edits files himself. Ask before assuming he wants automation.
- "Cages" = cars.

### Session hygiene — the failure modes that repeat

- Do not treat each reply as self-contained. This is one stateful session.
  Track what's been established, what's been tried, what failed.
- Before proposing a fix, ask: "have I confirmed the current state, or am
  I assuming it?"
- Before saying "this is a bug", ask: "could this be load-bearing for a
  reason I don't see?"
- When I paste output, read all of it. The answer is usually in the line
  you skimmed.

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
- On next wake, the maint block in `loop()` brings WiFi up (which stops
  BLE), starts an HTTP server, enters maint for up to `MAINT_MAX_MS`
  (15 min). If the wake was maint-bound, BLE is never brought up.
- During maint: BLE off, WiFi up, serial page live, OTA available via
  HTTP POST. OLED stays lit. Sensors sampled at 500 ms.
- Exit: HTTP `/maint/off`, engine start detected (voltage crosses
  `runningEnter_mv`), or 15-min timeout.
- See `MAINTENANCE.md` for full detail.

### HTTP endpoints (only during maint)

| Route | Method | Purpose |
|---|---|---|
| `/serial` | GET | HTML serial page |
| `/serial-raw` | GET | Text serial dump |
| `/maint/off` | POST | Exit maint |
| `/ota` | POST | Firmware upload |
| `/version` | GET | Current firmware version |
| `/ota-progress` | GET | OTA progress JSON (stage, bytes, total, countdown) |

### OTA

OTA is HTTP, only available in maint mode. GUI POSTs the compiled `.bin`
to `/ota?ver=X.YZ`. Firmware streams it into `Update.h` and reboots.
During the POST the GUI polls `/ota-progress` for stage transitions and
prints total elapsed time. (Percent-complete not available — the Arduino
`WebServer` strips `Content-Length` during multipart upload.)

The old BLE-URL OTA path is retired. `OtaManager.cpp` is legacy.

---

## Protected systems (do not casually modify)

- **OLED** — pins (6, 9), library, init, timing
- **Sleep/wake** — wake intervals, RTC state, sleep gate conditions
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify protocol
- **WiFi bring-up** — TX power must stay 8.5 dBm on this board
- **Upload pipeline** — 302 handling, URL-encoded POST format
- **Maintenance mode** — flag pattern (BLE set, next-wake consume),
  `MAINT_MAX_MS` cap, server lifecycle, skip-BLE-on-maint-wake
- **WiFi coexistence on C3** — 8.5 dBm TX, BLE teardown before WiFi init,
  do not touch `wifiBringUp()` sequence blindly. Two days and four AIs
  went into getting this stable.

Any change to these must be tested end-to-end on real hardware.

---

## What's next

See `TODO.md` for the full ranked list. Short version:

1. Set `BENCH_MODE 0` before deployment
2. Fix `readSensors()` early-return
3. Measure sleep current (MP1584EN swap if needed)
4. Wire GPS, test outdoors
5. Complete the switched 12V rail
6. Field install + 4-week baseline

---

## Known open issues

- `BENCH_MODE 1` compiled in for dev — do not deploy
- `readSensors()` early-return leaves stale voltage on fault
- MP1584EN quiescent current unmeasured
- GPS not wired
- WiFi NTP is a hard gate on maint entry — if NTP fails, maint fails
- `_mailFailCount` has no decay
- 302 handled as success without body check
- GUI maint state doesn't auto-flip to OFF when bike exits via engine-start bail
- GUI ride pull races service discovery — `[RIDE] pull err: Service Discovery`
- HTTP OTA percent progress not available (Content-Length stripped)

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
