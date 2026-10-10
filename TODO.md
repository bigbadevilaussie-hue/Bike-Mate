# Bike-Mate — TODO

Live task list. Ordered by priority within each section.

**Current:**
- Firmware on bike: V5.24 (needs flash — V5.23 on bike now)
- Firmware on disk: V5.24
- GUI on disk: V4.38

## Immediate

- [ ] **Flash V5.24 to bike.** Changes: `"st"` in publishBLE JSON (storage
  percent). Verify GUI `💾 Stor` badge populates with real % after BLE reconnect.
- [ ] **`/clock` POST doesn't stick when a BLE peer is connected.** The Mac
  GUI's `TimeCallbacks::onWrite` (BleManager.cpp:326) re-syncs `macTimeEpoch`
  the moment BLE comes back after maint exit (`wifiBringDown()` → `bleStart()`).
  A `/clock` POST during maint sets the value, then BLE reconnect clobbers it.
  Blocks the 4am scheduled-path test.
  Options: reject BLE time sync while a test-clock is latched; or add a
  `/clock?lock=1` flag that disables BLE time sync until reboot; or quit the
  GUI during the test.
- [ ] **4am scheduled path — first test.** Requires fix above. Sequence:
  1. Enter maint (GUI)
  2. Quit GUI so no BLE peer clobbers the clock
  3. `curl -X POST .../clock --data <03:59 today>`
  4. `curl -X POST .../maint/off`
  5. Watch next wake for `[UPLOAD] ====== ENTERING UPLOAD MODE ======`

## Test 4am scheduled upload path

- [ ] **Never tested.** V5.21 removed bench auto-upload; uploads now fire
  only at 4am (scheduled) or via the GUI force button. The force path is
  verified. The scheduled path is not.
- [ ] **Blocker: `last_epoch` gate.** `driveUploadShouldRun()` compares
  `getLastUploadEpoch()` against `mostRecent4am()`. After a forced upload,
  `last_epoch` is set to the (real) current epoch. A subsequent `/clock`
  POST that rolls the clock back to 03:59 leaves `last_epoch` ahead of the
  fake 4am, so the gate stays shut. Test therefore never fires.
- [ ] **Fix: clear `last_epoch` on clock rollback.** In `handleClockPost`
  (WebServer.cpp), if `e < prevEpoch`, also `prefs.putUInt("last_epoch", 0)`
  in the `upload` NVS namespace. Dormant in production (`/clock` never
  called in normal use). Lets the 4am test be run in one maint session.
- [ ] **Test procedure once fix lands:**
  1. Enter maint via GUI
  2. Quit the GUI so no BLE peer re-syncs the clock (BleManager.cpp:326
     `TimeCallbacks::onWrite`)
  3. `curl -X POST .../clock --data <03:59 today AEST>`
  4. Verify serial shows `[CLOCK] rollback ... cleared last_epoch`
  5. `curl -X POST .../maint/off`
  6. Watch next wake for:
     - `[UPLOAD] ====== ENTERING UPLOAD MODE (scheduled) ======`
     - `[CLOCK] from LAN` (clock re-synced inside upload path — V5.25)
     - `[WAKE] rotating at <epoch>` after the clock sync
     - `[UPLOAD] PUT WAKES/... OK (code=201)`
     - `[UPLOAD] ====== OK (ok=N fail=0) ======`
     - `[WIFI] down` after (scheduled path tears down WiFi)
- [ ] **Rollback safe in production:** `/clock` is only callable during
  maint, and normal usage never calls it. No behaviour change for field
  deployment.

## Cold-boot clock pair mismatch

- [ ] **`clockRestore()` leaves `secondsAtSync` stale after cold boot.**
  On cold boot, `totalSeconds` is reset to 0 but `clockRestore()` only
  restores `macTimeEpoch` and `secondsAtSync` from NVS. Result:
  `currentEpoch() = restoredEpoch + (0 - restoredSecondsAtSync)`, off by
  however many seconds were in `secondsAtSync` at save time. Observed
  in V5.24 boot: `[CLOCK] restored from NVS epoch=1791602784` followed by
  `[WAKE] sample @ 1791600036` — 2748 s hole. Self-heals on maint entry
  when Opal clock syncs, but wrong until then.
  Fix: in the cold-boot block, after `clockRestore()` succeeds, set
  `secondsAtSync = totalSeconds` to align the pair.

## Clock — GPS source

- [ ] **Parse `$GPRMC` UTC time + date** in `GpsModule.cpp`. Field 1 (hhmmss) +
  field 9 (ddmmyy). Only on status `A`.
- [ ] **Compute UTC epoch** and store in `_epochUTC`. Bounds check
  `1700000000 < e < 4102444800`.
- [ ] **`gpsEpochUTC()` accessor** — declared already, just needs to return
  `_epochUTC`.
- [ ] **At engine stop, after `closeRideLog()`:** read `gpsEpochUTC()`, if valid
  update `macTimeEpoch` + `secondsAtSync`. Serial log:
  `[CLOCK] ride close sync old=<a> new=<b>`.
- [ ] **Seal first, then update epoch** — ride file stays internally consistent,
  no Dr Who.

## Seal-before-upload restructure

- [ ] **Move `wakeLoggerForceRotate()`** out of `doStateWork()` into
  `driveUploadPerform()`, after WiFi up and clock sync. Seal uses corrected
  epoch.
- [ ] **Call `clockBringUp()` inside `driveUploadPerform()`** after
  `wifiBringUp()` succeeds, before seal. Opal HTTP first, NTP fallback.
- [ ] **`setLastUploadEpoch()` uses corrected clock** — automatic once clock
  sync precedes it.
- [ ] **`# closed=<epoch> (YYYY-MM-DD HH:MM)` line** — new
  `wakeLoggerLogClose()` in `WakeLogger.cpp`, called before seal. Same format
  as `# opened=`.

## Ride row format expansion

- [ ] **Add accessor `gpsCourseDeg()`** in `GpsModule.cpp` — parse `$GPRMC`
  field 8 (course made good, true).
- [ ] **Extend `writeRideRow()`** in `RideLogger.cpp` — emit
  `speed,hdg,alt,sats,fix` after `state`.
- [ ] **Update header line** in `RideStorage.cpp writeHeader()` — match new
  column order.
- [ ] **Extend BLE row payload** in `BleManager.cpp pushNewSlots()` — 8 bytes
  → 16 bytes.
- [ ] **Update `bikemate/ble.py parse_row()`** — parse 16-byte payload.
- [ ] **Update `bikemate/helpers.py parse_row()`** — new row struct.
- [ ] **Update `bikemate/reports.py` CSV parse** — expect new columns.

## Wake file clock-sync marker (decide)

- [ ] **Decide:** log the ride-close clock sync to serial only, or write
  `# clock_sync old=<a> new=<b>` into the wake file.
- [ ] **Implement whichever.**

## Docs

- [ ] **Update `README.md`** — note GPS as clock source, seal-before-upload
  flow, `# closed=` line, new ride row format.
- [ ] **Update `DYNA_TUNE.md`** — Dyna Tune should parse the new ride row
  columns.
- [ ] **`HANDOFF.md`** — refresh "current state" and "known open issues".
- [ ] **Fix README duplication** — Maintenance / GUI / Bench snapshot /
  Protected systems / Prime directive all appear twice.

## Housekeeping

- [ ] **`ESP32-C3` XCW board** — replace with new board when the MP1584EN
  modules arrive.
- [ ] **Voltage divider calibration** — measure actual ratio with DMM, update
  `BATTERY_SLOPE`.
- [ ] **NTC constants** — code has B=4600, physical part is B=3950. Fix or
  accept ~1.5 °C error.

## Deferred

- [ ] **Summary struct expansion** — `avgSpeed`, `maxSpeed`, `maxAlt`,
  `minAlt` using spare fields.
- [ ] **DS3231 hardware RTC** — for multi-week accuracy without GPS or WiFi.
- [ ] **Dyna Tune `dynatune.py`** — real analysis module against the spec.
- [ ] **Settings History window** — logging layer + Tk window.
- [ ] **Power measurement** — MP1584EN quiescent at 12.6 V input, 5 V out.

## Blockers

- MP1584EN modules in transit (arriving Mon 12 – Tue 13 Oct).
- New ESP32-C3 board needed for reliable bench work.
- GPS outdoor test still pending.

## Done (recent)

- [x] **V5.23 — force upload via HTTP `/upload` during maint.** Verified working
  end-to-end: GUI POSTs, bike runs `driveUploadPerform(true)`, WiFi stays up,
  server survives. Rule 7 still correct.
- [x] **V5.23 — remove BLE force-upload handler.**
- [x] **V5.23 — `fromMaint` param** on `driveUploadPerform()` to skip
  `wifiBringDown()` when called from the maint HTTP path.
- [x] **V5.24 — `"st"` storage percent** in `publishBLE()` JSON.
- [x] **GUI 4.38 — `💾 Stor` badge** in the Temp/Time row.
- [x] **README — zsh bracketed-paste note, patch anchor rules.**
