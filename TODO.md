# Bike-Mate — TODO

Live task list. Ordered by priority within each section.

## Immediate (V5.21)

- [ ] **Seal-before-upload restructure** — move `wakeLoggerForceRotate()` out of
  `doStateWork()` into `driveUploadPerform()`, after WiFi up and clock sync.
  Seal uses corrected epoch.
- [ ] **Clock sync in upload path** — call `clockBringUp()` inside
  `driveUploadPerform()` after `wifiBringUp()` succeeds, before seal.
  Opal HTTP first, NTP fallback.
- [ ] **`setLastUploadEpoch()` uses corrected clock** — automatic once clock
  sync precedes it.
- [ ] **`# closed=<epoch> (YYYY-MM-DD HH:MM)` line** — new
  `wakeLoggerLogClose()` in `WakeLogger.cpp`, called before seal. Same format
  as `# opened=`.
- [ ] **Version bump to 5.21** in `Config.h`.

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

## Ride row format expansion (V5.22)

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
- [ ] **Version bump to 5.22**.

## Storage visibility

- [ ] **Add `"st":<percent>`** to `publishBLE()` JSON in `BleManager.cpp`.
- [ ] **GUI reads `state.latest_data["st"]`** — badge / colour change above 75%.
- [ ] **GUI 4.36 bump** in `bikemate/config.py`.

## Force upload

- [ ] **BLE command `{"upload":"now"}`** in `OtaCallbacks::onWrite` — sets
  `uploadRequested = true`.
- [ ] **GUI menu item** "Force Upload" under Maintenance.
- [ ] **`bikemate/ble.py` send method** for the new command.

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
