# Bike-Mate — Peer Review Audit

**Date:** 2026-10-01
**Firmware reviewed:** V4.21 (Config.h) / GUI V3.11
**Reviewers:** Claude, Gemini, ChatGPT, Mistral (independent desk checks)
**Reviewed by:** bigbadevilaussie-hue

---

## Method

All four AIs were given the same prompt: read every file, give opinions and
issues, no code, no rewrites. Each returned an independent review. This
document consolidates their findings into a single table and ranks issues by
cross-reviewer consensus.

Legend: ✅ = flagged | — = not flagged

---

## Consolidated issue table

| # | Issue | Claude | Gemini | ChatGPT | Mistral | Consensus |
|---|---|---|---|---|---|---|
| 1 | PANIC blocks sleep — death spiral, prime directive violated | ✅ | ✅ | ✅ | ✅ | **4/4** |
| 2 | OTA MD5 verified AFTER `Update.end(true)` | ✅ | — | ✅ | ✅ | **3/4** |
| 3 | NVS ride summary accumulation (no pruning) | ✅ | — | — | ✅ | **2/4** |
| 4 | LittleFS exhaustion during offline periods | ✅ | ✅ | — | ✅ | **3/4** |
| 5 | BLE row `sats` vs `state` protocol mismatch | ✅ | — | — | — | **1/4** |
| 6 | Upload is time-driven, not battery-driven | ✅ | — | — | ✅ | **2/4** |
| 7 | `shouldSleep()` BLE-connected rule odd | ✅ | ✅ | — | — | **2/4** |
| 8 | `gpsHasFix()` in DisplayManager lies (duplicate stub) | ✅ | — | — | ✅ | **2/4** |
| 9 | `readSensors()` early-return leaves stale voltage | ✅ | — | ✅ | ✅ | **3/4** |
| 10 | Three voltage calibration systems — only wrong one live | ✅ | — | ✅ | ✅ | **3/4** |
| 11 | `Config.local.h` credentials in dump | — | — | ✅ | ✅ | **2/4** |
| 12 | No max-awake timeout | — | — | — | — | **0/4** (project add) |
| 13 | 100nF cap missing on voltage divider | — | — | — | — | **0/4** (known) |
| 14 | No fuse on 12V input | — | — | — | — | **0/4** (hardware) |
| 15 | Version chaos across all docs | ✅ | ✅ | — | ✅ | **3/4** |
| 16 | OLED pin contradiction (SDA 6 vs 8) | ✅ | — | — | ✅ | **2/4** |
| 17 | Ride interval 5s vs actual 30s (TICK_MS) | — | — | ✅ | ✅ | **2/4** |
| 18 | Upload 302 treated as unconditional success | ✅ | — | ✅ | ✅ | **3/4** |
| 19 | `newest_epoch` cleared on partial failure | — | — | ✅ | ✅ | **2/4** |
| 20 | postFile loads whole file + 3× URL-encoded copy in RAM | ✅ | — | ✅ | ✅ | **3/4** |
| 21 | OTA timeout doesn't actually supervise (blocking) | — | — | — | ✅ | **1/4** |
| 22 | OTA unauthenticated (no signing, MD5 from same source) | — | — | — | ✅ | **1/4** |
| 23 | `_mailFailCount` permanent kill switch (no decay) | ✅ | — | ✅ | ✅ | **3/4** |
| 24 | PANIC sends no email (`atRest` gate) | — | — | ✅ | ✅ | **2/4** |
| 25 | `lowVoltLoops` counts 50ms loops not wakes | — | — | ✅ | ✅ | **2/4** |
| 26 | `LittleFS.begin(true)` formats on corruption | — | — | ✅ | ✅ | **2/4** |
| 27 | BENCH_MODE 1 compiled in | ✅ | ✅ | ✅ | ✅ | **4/4** |
| 28 | Apps Script URL unauthenticated (public read/write) | — | — | ✅ | ✅ | **2/4** |
| 29 | Clock drift during deep sleep (no RTC crystal) | — | ✅ | — | ✅ | **2/4** |
| 30 | No autonomous time bootstrap on cold boot | ✅ | — | — | ✅ | **2/4** |
| 31 | WiFi failure leaves BLE suspended (asymmetric cleanup) | — | — | — | ✅ | **1/4** |
| 32 | TX power doc contradiction (13 vs 8.5 dBm) | ✅ | — | — | ✅ | **2/4** |
| 33 | GUI pull timeout 35s vs firmware push 41s (truncation) | — | — | ✅ | ✅ | **2/4** |
| 34 | WakeLogger `.sealed` rotation guard = good | ✅ | — | — | ✅ | **2/4** |
| 35 | `wakeLoggerRotate()` dead code with data-loss bug | ✅ | — | — | ✅ | **2/4** |
| 36 | Ride rows always `lat/lon = 0`, `state = 1` | ✅ | — | — | ✅ | **2/4** |
| 37 | `setRideStartLocation()` never called | ✅ | — | — | ✅ | **2/4** |
| 38 | `MAX_ROWS_PER_RIDE` not enforced | — | — | ✅ | ✅ | **2/4** |
| 39 | Ride file handle held open entire ride — power cut loses data | — | — | — | ✅ | **1/4** |
| 40 | Engine detection is voltage-only (false positives) | ✅ | — | — | ✅ | **2/4** |
| 41 | Engine-start check doesn't exclude `isArmingCountdown` — rebound bug | — | — | — | ✅ | **1/4** |
| 42 | `currentStateString()` doesn't report ARMING | — | — | — | ✅ | **1/4** |
| 43 | Mail threshold 12.5V only 0.5V above panic | ✅ | — | — | ✅ | **2/4** |
| 44 | Panic threshold 12.0V below stated 12.2V crank floor | ✅ | — | ✅ | ✅ | **3/4** |
| 45 | Three calibrations in Config — most defines dead | — | — | ✅ | ✅ | **2/4** |
| 46 | `latestMilliVolts = raw * 0.728` third slope | — | — | — | ✅ | **1/4** |
| 47 | No plausibility window / jump filter implemented | — | — | — | ✅ | **1/4** |
| 48 | Buzzer alarm + low-batt beep can overlap | — | — | — | ✅ | **1/4** |
| 49 | NTC constants in Sensors.cpp not Config.h | ✅ | — | — | ✅ | **2/4** |
| 50 | Blocking mail/upload/OTA freezes loop, epoch drifts | — | — | ✅ | ✅ | **2/4** |
| 51 | OTA ACK before fetch attempt — false "acknowledged" | — | — | — | ✅ | **1/4** |
| 52 | OTA can be triggered mid-ride (kills ride) | — | — | — | ✅ | **1/4** |
| 53 | GPS stub pollutes real data (fake coords in CSV) | — | — | — | ✅ | **1/4** |
| 54 | ACC reported as real, but only LED driven | — | — | — | ✅ | **1/4** |
| 55 | SCL on GPIO9 = boot strap pin, needs comment | — | — | — | ✅ | **1/4** |
| 56 | No long-park mode (night interval is only concession) | — | — | ✅ | ✅ | **2/4** |
| 57 | Trickle tender can fake engine start | — | — | — | ✅ | **1/4** |
| 58 | Rides shorter than wake interval don't exist | — | — | ✅ | ✅ | **2/4** |
| 59 | NVS namespace defines hardcoded elsewhere | — | — | — | ✅ | **2/4** |
| 60 | Ride CSV uploaded without summary/min/max/flags | — | — | — | ✅ | **1/4** |
| 61 | `rideStorageEnumerate()` dead code | — | — | — | ✅ | **1/4** |
| 62 | `WAKE_LOG_INTERVAL_SEC` unused | — | — | — | ✅ | **1/4** |
| 63 | `MAIL_FALLBACK_DAYS` unused | ✅ | — | — | ✅ | **2/4** |
| 64 | `FLAG_THERMAL_CUT` has no producer | — | — | — | ✅ | **1/4** |
| 65 | RideSummary doesn't set FLAG_PANIC on panic-close | — | — | — | ✅ | **1/4** |
| 66 | GUI churn — device sleeps at 60s, GUI reconnects forever | — | — | — | ✅ | **1/4** |
| 67 | OTA URL depends on `Familys-iMac.local` mDNS | — | — | — | ✅ | **1/4** |
| 68 | GUI two HTTP servers on 0.0.0.0 unauthenticated | — | — | — | ✅ | **1/4** |
| 69 | SSL verification disabled in GUI | ✅ | — | — | ✅ | **2/4** |
| 70 | Weather API over plain HTTP | ✅ | — | — | ✅ | **2/4** |

---

## Summary by consensus tier

| Tier | Count | Description |
|---|---|---|
| **4/4 (universal)** | 2 | Treat as fact |
| **3/4 (strong)** | 10 | Treat as real |
| **2/4 (partial)** | 30 | Verify each |
| **1/4 (single source)** | 24 | Check if real |
| **0/4 (project-added)** | 4 | Not caught by reviewers |

---

## Ranking: Top 20 by consensus × severity

| Rank | Issue | Consensus | Severity |
|---|---|---|---|
| 1 | PANIC blocks sleep — death spiral | 4/4 | Critical |
| 2 | BENCH_MODE 1 compiled in | 4/4 | Critical (if deployed) |
| 3 | OTA MD5 after `Update.end(true)` | 3/4 | Critical |
| 4 | 302 = unconditional success, deletes local file | 3/4 | Critical (data loss) |
| 5 | Three voltage calibrations, wrong one live | 3/4 | Critical (all thresholds wrong) |
| 6 | `readSensors()` early-return leaves stale voltage | 3/4 | Critical (silent failure) |
| 7 | LittleFS exhaustion during offline | 3/4 | Critical (long-term) |
| 8 | Panic threshold 12.0V below crank floor 12.2V | 3/4 | Critical (prime directive) |
| 9 | `_mailFailCount` permanent kill switch | 3/4 | High (silent alert loss) |
| 10 | postFile RAM blow-up | 3/4 | High (OOM crash) |
| 11 | Version chaos across docs | 3/4 | High (misleads future sessions) |
| 12 | `newest_epoch` cleared on partial failure | 2/4 | High (GUI push contract) |
| 13 | `Config.local.h` credentials exposed | 2/4 | High (rotate now) |
| 14 | Apps Script URL unauthenticated | 2/4 | High (public Drive) |
| 15 | `gpsHasFix()` duplicate stub lies on OLED | 2/4 | Medium |
| 16 | Ride interval 5s vs actual 30s | 2/4 | Medium |
| 17 | PANIC sends no email | 2/4 | Medium |
| 18 | `lowVoltLoops` = 100ms not 2 samples | 2/4 | Medium |
| 19 | `LittleFS.begin(true)` formats on corruption | 2/4 | Medium |
| 20 | No long-park mode | 2/4 | Medium |

---

## Unique findings — one reviewer only

| Issue | Only flagged by |
|---|---|
| BLE `sats` vs `state` mismatch | Claude |
| OTA timeout doesn't supervise | Mistral |
| OTA unauthenticated source | Mistral |
| WiFi failure leaves BLE suspended | Mistral |
| Ride file handle held open — power cut loses data | Mistral |
| Engine-start rebound during arming | Mistral |
| `currentStateString()` missing ARMING | Mistral |
| Buzzer overlap | Mistral |
| OTA ACK before fetch | Mistral |
| OTA mid-ride reboot kills ride | Mistral |
| GPS stub pollutes uploaded data | Mistral |
| ACC reported real but LED only | Mistral |
| SCL GPIO9 boot strap note | Mistral |
| Tender fakes engine start | Mistral |
| Ride CSV lacks summary | Mistral |
| `rideStorageEnumerate()` dead code | Mistral |
| `WAKE_LOG_INTERVAL_SEC` unused | Mistral |
| `FLAG_THERMAL_CUT` no producer | Mistral |
| RideSummary no FLAG_PANIC | Mistral |
| GUI churn connect/disconnect | Mistral |
| mDNS dependency on iMac | Mistral |
| GUI HTTP servers on 0.0.0.0 | Mistral |

Mistral found the most *unique* issues (22). Claude found the most *critical*
issues (protocol mismatch, GPS duplicate stub, BLE sleep rule).

---

## What none of them flagged

Issues noted in project conversation but not surfaced by any reviewer:

| Issue | Why it matters |
|---|---|
| No max-awake timeout | If anything blocks sleep, device stays awake at 40mA indefinitely |
| 100nF cap missing on voltage divider | Known hardware gap |
| No fuse on 12V input | Hardware — one short kills the buck |
| The 5-minute field wake may miss short rides entirely | Only ChatGPT & Mistral touched this |
| The rocker switch is the actual prime directive enforcement | All reviews miss that the human backstop is the real protection |
| The MP1584EN buck quiescent is unmeasured | The actual sleep number is still unknown |

---

## Verdict across all four

**Three of four AIs** independently reached the same top-3:
1. PANIC/sleep death spiral — 4/4
2. Voltage calibration wrong — 3/4
3. OTA MD5 after commit — 3/4

**Two of four** added:
4. LittleFS exhaustion — 3/4
5. 302 success + file deletion = data loss — 3/4

**One of four (Mistral)** went deeper into field failure modes than the others
combined.

The **universal** finding — 4/4 across all reviewers — is that the **prime
directive is actively violated by the current code**, not merely unimplemented.
That's the headline.

---

## Action items (derived)

Ordered by consensus × severity:

1. **Fix PANIC behaviour** — make it the cheapest state, not the most expensive
2. **Set BENCH_MODE 0** before any field deployment
3. **Move MD5 verification before `Update.end(true)`** — or use `Update.end(false)` then verify
4. **Verify the 302 body** or add a second confirmation step before deleting local files
5. **Establish one true voltage calibration** — bench PSU + multimeter, single source of truth
6. **Fix `readSensors()` early-return** — invalidate voltage on fault
7. **Add LittleFS free-space check** — refuse to start a ride if below threshold
8. **Raise panic threshold** to 12.2V to match crank floor
9. **Make `_mailFailCount` decay with time** — or reset per wake
10. **Stream uploads** — don't buffer whole file + URL-encoded copy
11. **Reconcile documentation** — README, PROJECT_STATE, HANDOFF, FILES

---

*End of audit.*
