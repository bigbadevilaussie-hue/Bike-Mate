# Bike-Mate — TODO

Live task list. Ordered by priority within each section.

**Current:**
- Firmware on bike: **V5.30**
- Firmware on disk: **V5.30** (committed, pushed, tagged)
- GUI on disk: **V4.38**
- Storage on bike: **~15%** (self-cleaning after every upload run)

---

## Immediate

- [ ] **Verify V5.30 upload run on the bike.** Full sequence:
  1. Enter maint
  2. `curl -X POST http://192.168.8.196/upload`
  3. Watch for: `manual seal` → `# closed=` → `gzip` → `removed raw` →
     `new file` → (no reconcile for current open file) → uploads with
     no 422 → paired raw deletes → `OK`
  4. Check LittleFS after: only `wakes_YYYY-MM-DD.csv` + newest ride
     `.csv` + newest ride `.csv.gz`

- [ ] **Verify scheduled 4am path seals the wake file.** V5.29 moved the
  seal into `driveUploadPerform()`, so both paths should seal. The test
  at 17:00 earlier was before the move. Needs re-test.

- [ ] **GUI `/upload` timeout is a false negative.** Firmware returns 200
  from `handleUploadNow()` immediately, but the 200 sits in the TCP
  buffer while `driveUploadPerform()` runs (no `serverLoop()` during the
  upload). GUI's 60s `urllib` timeout fires. Upload completes anyway.
  Cosmetic. Options: bump GUI timeout, add `serverLoop()` calls inside
  the upload, or accept and document.

---

## Ride lifecycle

- [ ] **Ride start 15s debounce.** Currently: 30s settle countdown from
  engine-start detection to `startRideLog()`. Spec: 15s of continuous
  voltage above `runningEnter_mv`. `rideStartEpoch` = moment the window
  opens (crank-detected), or the moment it closes (debounce-complete) —
  decide.
- [ ] **Ride end 15s debounce.** Currently: `parkedDelayMs = 0`, one low
  sample ends the ride. Spec: 15s of continuous voltage below
  `runningExit_mv`.
- [ ] **Re-entry guards.** `closeRideLog()` sets `isLogging = false`
  before writing the summary. A mid-write engine-start can create a
  second file. Add a "closing in progress" latch.
- [ ] **Clear `rideStartLat_x1e7` / `rideStartLon_x1e7` at
  `startRideLog()`.** They're non-RTC globals, currently never reset
  between rides. A ride starting without a GPS fix inherits the
  previous ride's `# start_loc=`.

---

## Ride row format expansion

Current 6-column format: `epoch,lat,lon,volt,temp,state`.
Target 11-column: add `speed,hdg,alt,sats,flags`.

- [ ] **`writeRideRow()`** in `RideLogger.cpp` — emit the extra fields.
- [ ] **`RideStorage.cpp writeHeader()`** — match new column order.
- [ ] **`RideStorage.cpp rideStorageAppendRow()`** — new format.
- [ ] **`RideRow` struct** in `Config.h` — add the fields.
- [ ] **BLE row payload** in `BleManager.cpp pushNewSlots()` — 8 bytes
  → ~20 bytes.
- [ ] **`bikemate/ble.py parse_row()`** — parse new payload.
- [ ] **`bikemate/helpers.py parse_row()`** — new row struct.
- [ ] **`bikemate/reports.py` CSV parse** — expect new columns.

---

## Clock

- [ ] **GPS clock source.** `GpsModule.cpp` parses `$GPRMC` time and
  date into `_epochUTC`. `gpsEpochUTC()` accessor exists but always
  returns 0. Implement the parse + populate. At engine stop after
  `closeRideLog()`: read `gpsEpochUTC()`, if valid update
  `macTimeEpoch` + `secondsAtSync`. Serial log:
  `[CLOCK] ride close sync old=<a> new=<b>`.
- [ ] **Seal first, then update epoch.** Ride file stays internally
  consistent. Currently no update happens, so this is moot until GPS
  clock source lands.
- [ ] **Cold-boot clock pair mismatch.** On cold boot, `totalSeconds`
  is reset to 0 but `clockRestore()` only restores `macTimeEpoch` and
  `secondsAtSync` from NVS. Result: `currentEpoch()` = `restoredEpoch +
  (0 - restoredSecondsAtSync)`, off by the saved `secondsAtSync`.
  Observed: 2748s hole. Self-heals on next Opal/NTP sync. Fix: in the
  cold-boot block, after `clockRestore()` succeeds, set `secondsAtSync
  = totalSeconds`.

---

## Wake file format

- [ ] **Decide final shape.** Current: data rows + `# upload=...`
  event lines interleaved. V5.29 added the event lines. Earlier spec
  considered a "pure data log" with events going to serial only.
  Confirm which.
- [ ] **Reader must skip `#` lines when parsing rows.** `reports.py`
  already skips `# fw=` and `# opened=`. Add `# upload=` and `# closed=`
  to the skip set if not already covered by a general `#` check.

---

## Upload / storage (mostly done this session)

- [x] **Force upload via HTTP `/upload` during maint.** V5.23. Verified.
- [x] **BLE force-upload handler removed.** V5.23.
- [x] **`fromMaint` param** on `driveUploadPerform()`. V5.23.
- [x] **Clock sync inside `driveUploadPerform()`.** V5.25.
- [x] **Upload reason param** (`forced` / `scheduled`). V5.25.
- [x] **Storage % telemetry.** `"st":<percent>` in publishBLE. V5.24.
- [x] **GUI Stor badge.** V4.38.
- [x] **Ride `# opened=` / `# pre_ride_volt=` / `# start_loc=` /
  `# closed=`.** V5.27.
- [x] **gzip every file, no threshold.** V5.27.
- [x] **OTA OLED redraw during upload.** V5.27.
- [x] **Debug heartbeats + per-stage timing.** V5.27.1.
- [x] **`/seal` manual endpoint.** V5.28.
- [x] **Monotonic `_epochFloor`.** V5.28.
- [x] **Upload routine: seal → reconcile → upload → delete raw.**
  V5.29.
- [x] **`# upload=...` event lines in wake file.** V5.29.
- [x] **Reconcile skips current open files.** V5.30.
- [x] **422 treated as already-exists.** V5.30.
- [x] **Raw `.csv` / `.sealed` deleted after gzip.** V5.30.

- [ ] **Orphan ride summary recovery.** When a ride `.csv` exists with
  no NVS summary (power-off mid-ride), reconcile should parse the file
  and write a summary. Not implemented. Currently the file uploads raw
  and the GUI can't show it as Last Ride.

---

## Alarm / PANIC

- [ ] **Horn policy.** If a 12V horn is added as a PANIC output:
  horn fires once at latch, silence after. Subsequent wakes: mail only.
  Never repeat the blast. A 15A horn on every wake kills the battery
  in hours.
- [ ] **PANIC beep cadence.** Currently one beep sequence per wake
  while latched. Confirm this is wanted, or make it once-per-hour.
- [ ] **Mail on PANIC.** Fires once at latch (current). Confirm.
- [ ] **Arming chirp + fake alarm LED flash.** Cosmetic. Keep.

---

## GUI

- [ ] **`/upload` timeout false negative.** See Immediate.
- [ ] **`reports.py` skip `#` lines.** See Wake file format.
- [ ] **Drive sync.** Already pulls wakes + rides + bin. Verified.
  No change.

---

## Docs

- [ ] **`README.md`** — update for V5.30. `/seal` endpoint, upload
  routine flow, wake file `# upload=` lines, ride header fields.
- [ ] **`DYNA_TUNE.md`** — refresh.
- [ ] **`HANDOFF.md`** — refresh current state.
- [ ] **README duplication** — Maintenance / GUI / Bench snapshot /
  Protected systems / Prime directive. Still duplicated.

---

## Housekeeping

- [ ] **ESP32-C3 XCW board** — replace with new board when the
  MP1584EN modules arrive.
- [ ] **Voltage divider calibration** — measure actual ratio with DMM,
  update `BATTERY_SLOPE`.
- [ ] **NTC constants** — code has B=4600, physical part is B=3950.
  Fix or accept ~1.5 °C error.

---

## Deferred

- [ ] **Summary struct expansion** — `avgSpeed`, `maxSpeed`, `maxAlt`,
  `minAlt` using spare fields.
- [ ] **DS3231 hardware RTC** — multi-week accuracy without GPS or WiFi.
- [ ] **Dyna Tune `dynatune.py`** — real analysis module.
- [ ] **Settings History window** — logging layer + Tk window.
- [ ] **Power measurement** — MP1584EN quiescent at 12.6 V input.

---

## Blockers

- MP1584EN modules in transit.
- New ESP32-C3 board needed for reliable bench work.
- GPS outdoor test still pending.

---

## Done this session (2026-10-10)

- V5.23: HTTP `/upload` during maint, BLE force-upload handler removed
- V5.24: storage % telemetry + GUI Stor badge
- V5.25: clock sync inside `driveUploadPerform()`, upload reason
- V5.26: GPS debug print with utc/hdg/alt
- V5.27 / V5.27.1: ride header/footer stamps, gzip all, OTA OLED
  redraw, debug heartbeats
- V5.28: `/seal` endpoint, monotonic epoch floor
- V5.29: upload routine rebuild — seal, reconcile, upload, delete raw,
  `# upload=` event lines
- V5.30: reconcile skips current open files, 422 handled, raw delete
  after gzip in seal paths

**Verified working end-to-end:**
- Force upload (GUI → `/upload` → GitHub → GUI Drive sync)
- Scheduled upload at 17:00 test (before V5.29 seal move)
- Rule 7 hold-back of newest ride
- `/seal` manual seal: `# closed=`, gzip, raw delete, new file
- Reconcile: orphan `.csv` / `.sealed` → gzip → raw delete
- Storage reclaimed from 53% → 15% after V5.30 upload run
