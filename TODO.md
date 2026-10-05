# Bike-Mate — TODO

Ranked by verifiability, not by AI consensus. Categories:
- **A** = verifiable from code — real, you can check it
- **B** = depends on hardware/usage — may not apply
- **C** = reviewer misunderstood the project — ignore

---

## Tier 1 — Safety-critical, do before any field deployment

### 1. Set `BENCH_MODE 0`
- **Cat:** A
- **Effort:** 1 line
- **What:** `Config.h` currently has `BENCH_MODE 1`. If it boots on the bike,
  it wakes every 30 s and never triggers the 04:00 upload.
- **Done when:** `BENCH_MODE 0`, committed.

### 2. Fix `readSensors()` early-return
- **Cat:** A
- **Effort:** 3 lines
- **What:** `if (raw < 100) return;` leaves `latestBatteryVoltage` frozen at
  the last value. Sensor fault reads as healthy battery.
- **Fix:** Set `latestBatteryVoltage = 0` and a fault flag.
- **Done when:** Disconnecting the divider makes firmware report fault,
  not stale data.

### 3. Raise panic threshold to 12.2V
- **Cat:** A
- **Effort:** 2 lines in `Config.h`
- **What:** `V_PANIC_ENTER 12.0` is below the stated 12.2V crank floor.
  Panic fires after the prime directive is already violated.
- **Also:** raise `WARN_EMAIL_VOLTAGE` from 12.50 → 12.60
- **Done when:** Config updated, tested at PSU 12.3V.

---

## Tier 2 — Firmware correctness

### 4. Measure sleep current
- **Cat:** A
- **Effort:** Bench work
- **What:** The MP1584EN buck quiescent draw is unmeasured. The Mini 360
  was suspected 10 mA. If the sleep current is above ~5 mA the prime
  directive is not achievable.
- **How:** PSU 12 V → buck, DMM in series on the 12 V line, no load on 5 V.
  Then with ESP32 sleeping.
- **Done when:** Bench.md has actual measured numbers.

### 5. Field install prep
- **Cat:** B
- **Effort:** Multi-day
- **What:** Perfboard build. TVS fitted. Fuse inline. MOSFET ACC rail
  wired. GPS wired. GPS antenna positioned. MP1584EN swapped in if
  sleep current test justifies.
- **Done when:** 4-week baseline starts.

### 6. Wire GPS + outdoor walk test
- **Cat:** B
- **Effort:** Bench + outdoor
- **What:** GPS not yet wired. `GpsModule.cpp` is written but unproven.
- **Done when:** Fix acquired, lat/lon in ride log non-zero.

---

## Tier 3 — Robustness

### 7. `newest_epoch` only cleared on successful upload
- **Cat:** A (code clears it unconditionally)
- **Effort:** Small
- **What:** Cleared even on partial failure. GUI push contract breaks.
- **Done when:** Partial upload leaves `newest_epoch` intact.

### 8. `_mailFailCount` decay
- **Cat:** A (no decay logic in code)
- **Effort:** Small
- **What:** 3 failures = email dead until cold boot. The failure mode
  guarantees it: backoff blocks sends, so no send can succeed to clear
  backoff.
- **Fix:** Time-based decay, or reset per wake.
- **Done when:** Fail 3 times, wait 24 h, alert succeeds.

### 9. Add max-awake timeout for stuck maint
- **Cat:** A (maint already has one, general case does not)
- **Effort:** Small
- **What:** `MAX_AWAKE_MS` covers normal operation. Maint is exempt and
  uses `MAINT_MAX_MS` instead. But if any future feature keeps the
  device awake past its own budget, there's no backstop.
- **Done when:** Every awake path has a bounded cap.

### 10. 302 verification before deleting local files
- **Cat:** A (code treats 302 as success without checking)
- **Effort:** Medium
- **What:** Apps Script always returns 302. Actual errors hidden behind
  the redirect. Drive-side failure looks like success → local file deleted.
- **Fix:** Follow redirect, check body, or use a second endpoint.
- **Done when:** Simulated Drive failure doesn't delete local files.

### 11. `postFile()` RAM blow-up
- **Cat:** B (depends on file size in practice)
- **Effort:** Medium
- **What:** Loads whole file + ~3× URL-encoded copy. A big ride CSV will
  OOM on the C3.
- **Done when:** Can upload 100 KB file without crash.

### 12. Retire port-8000 OTA server from GUI
- **Cat:** A
- **Effort:** Small
- **What:** `start_ota_server()` / `stop_ota_server()` / `OTA_DIR` /
  `OTA_PORT` / `OTA_HOSTNAME` / `OTA_WAIT_SEC` are dead code now that
  OTA is HTTP POST from the GUI. Port 8000 is still bound but nothing
  serves from it.
- **Done when:** Dead code removed, port not bound.

### 13. Remove dead `send_ota_command` from `BLEWorker`
- **Cat:** A
- **Effort:** Small
- **What:** Nothing calls it after GUI 4.12. It still references
  `OTA_WAIT_SEC` and does the old BLE wait pattern.
- **Done when:** Method removed, no call sites.

---

## Tier 4 — Nice to have

### 14. Maint cap counter on serial page
- **Cat:** A
- **Effort:** Small
- **What:** Show `remaining Ns` in the maint log every 30 s, or as a
  header on `/serial-raw`. Useful for knowing how long until timeout.
- **Done when:** Visible during maint.

### 15. Serial page styling pass
- **Cat:** C
- **Effort:** Small
- **What:** Current page is functional but plain. Could add a compact
  header with firmware version, RSSI, uptime, remaining maint time.
- **Done when:** Looks nicer, still serves the same data.

### 16. Kill the `.sealed` upload race
- **Cat:** B
- **Effort:** Small
- **What:** Occasionally a `.sealed` file exists at boot, gets deleted
  immediately, but the upload may have already started on a different
  path. Rare.
- **Done when:** No `.sealed` files linger.

---

## Tier 5 — Deferred / depends on usage

### 17. PANIC sends email
- **Cat:** B — depends on whether the panic-email path is ever hit
- **What:** `atRest` gate requires `!inPanic`, so panic transition sends
  no email. Fixed in V4.72 by bypassing latches on PANIC wake.

### 18. `lowVoltLoops` debounce
- **Cat:** B — 100 ms debounce may be fine in practice
- **What:** Counts 50 ms loop passes. `LOW_VOLT_LOOPS_REQUIRED 2` = 100 ms,
  not 2 samples.
- **Verify:** Has a false alert ever occurred?

### 19. `LittleFS.begin(true)` formats on corruption
- **Cat:** B — depends on how often flash corrupts
- **What:** `begin(true)` formats on mount failure. One corruption wipes
  data history.
- **Verify:** Has a LittleFS corruption event ever been seen?

### 20. LittleFS free-space check before ride start
- **Cat:** B — only matters if uploads fail repeatedly
- **What:** No free-space check. Extended WiFi outage → files accumulate
  → silent failure.

### 21. NVS ride summary accumulation
- **Cat:** B — years-long problem
- **What:** Every ride leaves an `s<epoch>` blob forever.
- **Verify:** How many rides/year? 365 entries = fine for years.

### 22. `gpsHasFix()` duplicate stub
- **Cat:** A (code has two stubs) but cosmetic
- **What:** DisplayManager has `gpsHasFix() { return true; }` that ignores
  the real stub state.

### 23. Ride interval 5s vs actual 30s
- **Cat:** B — depends on whether 5 s was intended
- **What:** Config says `LOG_INTERVAL_SEC 5`, `writeRideRow()` only called
  at `TICK_MS 30000`.

### 24. `setRideStartLocation()` never called
- **Cat:** A (grep shows zero calls)
- **What:** GPS start location is always 0 in ride summaries.

---

## Done — historical

The following were done in previous sessions and no longer need work:

- PANIC sleep behaviour (V4.72)
- Max-awake timeout for normal operation (V4.72)
- BENCH_MODE 1 compiled in (V4.73 set 0, reverted for dev)
- `_mailFailCount` latches bypassed on PANIC wake (V4.72)
- 302 handling for upload (accepted)
- MD5 verification on OTA (accepted)
- Maintenance mode + serial page + HTTP OTA (V4.75-4.78)
- GUI HTTP OTA during maint (GUI 4.12+)

---

*End of TODO.*
