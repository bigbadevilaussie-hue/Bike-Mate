# Bike-Mate — TODO

Ranked by verifiability, not by AI consensus. Categories:
- **A** = verifiable from code — real, you can check it
- **B** = depends on hardware/usage — may not apply
- **C** = reviewer misunderstood the project — ignore

---

## Tier 1 — Verifiable from code, fix now

### 1. Set `BENCH_MODE 0` before any deployment
- **Cat:** A
- **Effort:** 1 line
- **What:** `Config.h` has `#define BENCH_MODE 1`. If it ever boots on the bike, it wakes every 30s and never triggers the 04:00 upload.
- **Done when:** `BENCH_MODE 0`, committed.

### 2. Fix `readSensors()` early-return
- **Cat:** A
- **Effort:** 3 lines
- **What:** `if (raw < 100) return;` leaves `latestBatteryVoltage` frozen at the last value. Sensor fault reads as healthy battery.
- **Fix:** Set `latestBatteryVoltage = 0` and a fault flag.
- **Done when:** Disconnecting the divider makes firmware report fault, not stale data.

### 3. Move MD5 verification before `Update.end(true)`
- **Cat:** A
- **Effort:** Small reorder in `OtaManager.cpp`
- **What:** Corrupt image is committed to the OTA partition before MD5 is checked. Next reboot boots the bad image.
- **Done when:** `Update.end(false)` → verify MD5 → commit. Or fetch to RAM, verify, then flash.

### 4. Raise panic threshold to 12.2V
- **Cat:** A
- **Effort:** 2 lines in `Config.h`
- **What:** 12.0V is below the stated 12.2V crank floor. Panic fires after the prime directive is already violated.
- **Also:** raise `WARN_EMAIL_VOLTAGE` from 12.50 → 12.60
- **Done when:** Config updated, tested at PSU 12.3V.

### 5. Fix PANIC sleep behaviour
- **Cat:** A (verifiable in code) — but whether it matters depends on usage
- **Effort:** Medium
- **What:** `shouldSleep()` requires `!inPanic`. PANIC = never sleeps = OLED + BLE + beeping at 40mA forever.
- **Fix:** Allow sleep in PANIC with a slower wake interval (e.g. 30 min). One email, then quiet.
- **Done when:** PSU at 11.9V → device sleeps, doesn't drain.

---

## Tier 2 — Verifiable but lower urgency

### 6. Reconcile documentation
- **Cat:** A (docs are verifiably stale)
- **Effort:** Medium
- **What:** README says V2.00, PROJECT_STATE says V3.50, HANDOFF says V3.29, code says V4.21. FILES.md says SDA=8, code says 6. PROJECT_STATE says TX 13dBm, code says 8.5.
- **Impact:** Misleads future AI sessions and yourself.
- **Done when:** All docs match current code. Add a `VERSION` file as single source.

### 7. Establish one voltage calibration
- **Cat:** A (three calibrations exist in code)
- **Effort:** Small bench work
- **What:** Config's `ADC_SLOPE`/`DIVIDER_RATIO 10.771` (unused), Sensors' `BATTERY_SLOPE 0.008058` (ideal 10:1, live), stated hardware (11:1). Wrong one is live.
- **Fix:** PSU 12.00V + multimeter, adjust slope until firmware matches, put winning numbers in Config.h only.
- **Done when:** One constant, one source, verified.

### 8. `newest_epoch` only cleared on successful upload
- **Cat:** A (code clears it unconditionally)
- **Effort:** Small
- **What:** Cleared even on partial failure. GUI push contract breaks.
- **Done when:** Partial upload leaves `newest_epoch` intact.

### 9. `_mailFailCount` decay
- **Cat:** A (no decay logic in code)
- **Effort:** Small
- **What:** 3 failures = email dead until cold boot. The failure mode guarantees it: backoff blocks sends, so no send can succeed to clear backoff.
- **Fix:** Time-based decay, or reset per wake.
- **Done when:** Fail 3 times, wait 24h, alert succeeds.

### 10. Add max-awake timeout
- **Cat:** A (not in code)
- **Effort:** Small
- **What:** If anything blocks sleep (stuck BLE client, stuck WiFi, stuck flag), device stays awake at 40mA indefinitely.
- **Fix:** `if (millis() - awakeStart > 120s) force sleep;`
- **Done when:** Stuck state doesn't prevent sleep.

### 11. 302 verification before deleting local files
- **Cat:** A (code treats 302 as success without checking)
- **Effort:** Medium
- **What:** Apps Script always returns 302. Actual errors hidden behind the redirect. Drive-side failure looks like success → local file deleted.
- **Fix:** Follow redirect, check body, or use a second endpoint.
- **Done when:** Simulated Drive failure doesn't delete local files.

### 12. `postFile()` RAM blow-up
- **Cat:** B (depends on file size in practice)
- **Effort:** Medium
- **What:** Loads whole file + ~3× URL-encoded copy. A big ride CSV will OOM on the C3.
- **Done when:** Can upload 100KB file without crash.

---

## Tier 2.5 — Board revision items (do on perfboard, not breadboard)

### Divider swap — 470k / 47k
- **Cat:** A (known values, known math)
- **Effort:** 2 resistors, next board revision
- **What:** Current divider is 98.8k + 9.98k = 108.78 kΩ → 116 µA continuous on 12 V. Swap to 470k + 47k = 517 kΩ → 24 µA. 80% reduction. Saves ~62 mAh over 4 weeks.
- **Why not 1M/100k:** same output voltage (11:1 ratio), but 11× over the ADC's recommended source impedance. The 100 nF cap keeps it stable, but calibration gets harder for an 8 mAh gain over 4 weeks. Not worth it.
- **Verify after swap:** PSU at 12.0 / 12.6 / 13.8 V, DMM on divider output, confirm reading matches firmware within ±0.1 V. Recalibrate BATTERY_SLOPE if needed.
- **Sequencing:** Do NOT touch the divider until MP1584EN is fitted and PANIC sleep is fixed. The divider is the last 3–4% of the budget. Buck and firmware first.
- **Done when:** 470k/47k fitted, calibrated, and 4-week average includes ~24 µA divider contribution in BENCH.md.

## Tier 3 — Depends on usage, verify first

### 13. PANIC sends email
- **Cat:** B — depends on whether the panic-email path is ever hit
- **Effort:** Small
- **What:** `atRest` gate requires `!inPanic`, so panic transition sends no email.
- **Verify:** Does a 12.1V transition trigger an alert in the actual build?

### 14. `lowVoltLoops` debounce
- **Cat:** B — 100ms debounce may be fine in practice
- **Effort:** Small
- **What:** Counts 50ms loop passes. `LOW_VOLT_LOOPS_REQUIRED 2` = 100ms, not 2 samples.
- **Verify:** Has a false alert ever occurred?

### 15. `LittleFS.begin(true)` formats on corruption
- **Cat:** B — depends on how often flash corrupts
- **Effort:** Small
- **What:** `begin(true)` formats on mount failure. One corruption wipes data history.
- **Verify:** Has a LittleFS corruption event ever been seen?

### 16. LittleFS free-space check
- **Cat:** B — only matters if uploads fail repeatedly
- **Effort:** Small
- **What:** No free-space check. Extended WiFi outage → files accumulate → silent failure.
- **Verify:** Do uploads actually fail long enough to fill the FS?

### 17. NVS ride summary accumulation
- **Cat:** B — years-long problem
- **Effort:** Medium
- **What:** Every ride leaves an `s<epoch>` blob forever.
- **Verify:** How many rides/year? 365 entries = fine for years.

### 18. `gpsHasFix()` duplicate stub
- **Cat:** A (code has two stubs) but cosmetic
- **Effort:** Small
- **What:** DisplayManager has `gpsHasFix() { return true; }` that ignores the real stub state.
- **Done when:** Single GPS source.

### 19. Ride interval 5s vs actual 30s
- **Cat:** B — depends on whether 5s was intended
- **Effort:** Small
- **What:** Config says `LOG_INTERVAL_SEC 5`, `writeRideRow()` only called at `TICK_MS 30000`.
- **Verify:** Is 5s or 30s desired for ride sampling?

### 20. `setRideStartLocation()` never called
- **Cat:** A (grep shows zero calls)
- **Effort:** Small
- **What:** GPS start location is always 0 in ride summaries.
- **Done when:** Either wire it up or remove the dead field.

---

## Tier 4 — Ignore unless proven relevant

| Issue | Cat | Verdict |
|---|---|---|
| GPS stub pollutes uploaded data | B | GPS isn't wired — deliberate |
| ACC reported real but LED only | B | MOSFETs unwired |
| Buzzer overlap | C | Cosmetic edge case |
| Engine-start rebound during arming | B | Only matters if it happens |
| `currentStateString()` missing ARMING | B | Cosmetic |
| Mail threshold 12.5V chatty | B | Depends on battery/climate |
| Ride CSV without summary | B | Depends on whether it's wanted |
| Trickle tender false positive | B | Only if a tender is used |
| Clock drift over deep sleep | B | Needs measurement |
| Long-park mode missing | B | Depends on rider behaviour |
| OTA mid-ride kills ride | B | Unlikely |
| OTA unauthenticated source | B | Hobby project, fine |
| GUI HTTP servers on 0.0.0.0 | B | LAN only, fine |
| SSL verify disabled | B | Fine for hobby |
| Weather API over HTTP | B | Cosmetic |
| `mDNS Familys-iMac.local` dependency | B | Works as-is |
| SCL GPIO9 boot strap comment | C | Note only |
| `latestMilliVolts = raw * 0.728` | C | Diagnostic only |
| `rideStorageEnumerate()` dead code | C | Harmless |
| `wakeLoggerRotate()` dead code | C | Harmless |
| `FLAG_THERMAL_CUT` no producer | C | Unused flag |
| `WAKE_LOG_INTERVAL_SEC` unused | C | Dead define |
| `MAIL_FALLBACK_DAYS` unused | C | Dead define |
| NVS namespace defines hardcoded | C | Cosmetic |
| Version chaos | A | Covered by #6 |

---

## Sprint Plan

### Sprint 1 — Safety (today, ~30 min)
- [ ] #1 Set `BENCH_MODE 0`
- [ ] #2 Fix `readSensors()` early-return
- [ ] #4 Raise panic threshold 12.0 → 12.2

**Commit:** V4.22 — "Safety-critical fixes"

### Sprint 2 — Firmware correctness (this weekend, ~2 hrs)
- [ ] #3 MD5 before commit
- [ ] #5 PANIC sleep behaviour
- [ ] #8 `newest_epoch` conditional clear

**Commit:** V4.23 — "Panic + OTA correctness"

### Sprint 3 — Calibration & docs (next week)
- [ ] #7 Voltage calibration with PSU + DMM
- [ ] #6 Reconcile docs

**Commit:** V4.24 + docs tag

### Sprint 4 — Robustness (following week)
- [ ] #9 Mail backoff decay
- [ ] #10 Max-awake timeout
- [ ] #11 302 verification
- [ ] #12 Stream uploads

**Commit:** V4.25

### Sprint 5 — Hardware bring-up
- [ ] #18 GPS duplicate stub
- [ ] #20 `setRideStartLocation()` wire or remove
- [ ] 100nF cap on voltage divider
- [ ] Measure sleep current
- [ ] Fix `NTC_B` to 3950 + recalibrate
- [ ] Wire MOSFET ACC
- [ ] Wire GPS

**Commit:** V5.00 breadboard complete

---

*End of TODO.*
