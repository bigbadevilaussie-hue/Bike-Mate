# Bike-Mate — Dyna Tune

Post-ride diagnostics and Settings History. Two connected features:

- **Dyna Tune** — KPI board that analyses a ride log (and optionally
  wake logs) against a fixed set of checks, producing a grid of
  PASS / WARN / FAIL / IDLE results plus recommendations.
- **Settings History** — a hypothesis register. Every settings change
  is logged with the reason it was made. When the reason was a Dyna Tune
  recommendation, the change is tracked to see whether it actually
  improved the target KPI.

Together they close the loop: `analyse → recommend → apply → measure →
validate or revert`.

The prime directive is unchanged — do not drain the bike battery past
crank. Dyna Tune is how the trend gets seen before the cliff.

## Status

Specification only. No code yet.

---

## 1. Reference: 2008 Triumph Sprint ST 1050

### 1.1 Charging system

| Parameter | Value |
|---|---|
| Charging voltage (running, idle) | **14.2 V** (observed, MOSFET R/R fitted) |
| Charging voltage (cruise, assumed) | 14.2–14.4 V — to be measured at 4K |
| Regulator/Rectifier | Biggiglife MOSFET (aftermarket, Apr 2022) |
| Stator output (AC, per phase) | ~20 V idle / ~70 V at 5K RPM |
| Fuse | 30 A main |

**Failure history on this bike:**

- Factory shunt R/R failed. Symptom progression over ~2 weeks: running
  voltage dropped from 14.2 V to resting-only ~12.5 V. Cranking weakened
  progressively. Final: battery below crank floor, stranded.
- MOSFET R/R fitted Apr 2022 to replace it. Currently ~3.5 years in
  service, working.

**Common Sprint failure modes:**

- Stator burnout — check AC output per phase
- R/R failure — open (no charge) or short (overcharge, boils battery)
- 3-pin stator-to-R/R connector melts — solder or replace
- R/R ground corrosion — clean to bare metal, add earth strap

### 1.2 Battery — fitted

| | |
|---|---|
| Model | SSB Powersport VTX12-BS V-Spec High Performance AGM |
| Spec | 12 V, 10.5 Ah, 180 CCA |
| Purchased | 07/06/2024 |
| Age | ~16 months |
| Location | Sideways tray, under seat |

**Healthy resting voltage observed:**

| Condition | Voltage |
|---|---|
| Immediately after ride | 12.9 V |
| 7 days later | 12.6 V |
| Running, idle | 14.2 V |

Decay rate observed: **~43 mV/day** over 7 days. Consistent with ~2 mA
average draw on a 10.5 Ah battery.

### 1.3 AGM voltage → state of charge

| Voltage (rested) | State of charge |
|---|---|
| 12.7–13.0 V | 100% |
| 12.6 V | 75% |
| 12.3 V | 50% |
| 12.0 V | 25% |
| 11.8 V | 0% |

**Practical crank floor: 12.2 V rested.** Below this, the 1050 triple
may not crank on a cold morning.

### 1.4 Location & climate — Atkinsons Dam QLD 4311

**Coordinates:** -27.4319, 152.4511

**Temperature:**

| | Jan | Feb | Mar | Apr | May | Jun | Jul | Aug | Sep | Oct | Nov | Dec |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Max °C | 31.5 | 30.7 | 29.5 | 27.1 | 23.7 | 21.1 | 20.7 | 22.4 | 25.5 | 28.1 | 30.1 | 31.3 |
| Min °C | 19.1 | 19.0 | 17.3 | 13.7 | 10.1 | 7.6 | 6.2 | 6.7 | 9.5 | 13.2 | 16.0 | 18.1 |

- Annual mean: ~26 °C max / ~13 °C min
- Days over 35 °C per year: **~16.5, concentrated Nov–Mar**
- Recorded extremes: 39.3 °C high, -4.4 °C low

**Rainfall:** ~770–990 mm/year. Wettest Dec–Mar. Driest Aug.

**Humidity:** ~72% annual mean.

**Wind:** ~26 km/h mean, gustier Nov.

**Climate implications for diagnostics:**

- **Winter:** battery capacity 70–80% of nominal at 6 °C. Resting V
  reads lower. Cranking sag deeper.
- **Summer:** heat soak in engine bay. R/R output drops slightly.
  Resting V reads artificially low for 30–60 min after a hot ride.
- **Humidity:** electronics need conformal coating or sealed enclosure.

### 1.5 Failure-to-diagnostic mapping

| Failure mode | First observable symptom | Diagnostic check |
|---|---|---|
| R/R open (no charging) | Running V = resting V | `charge_delta` |
| R/R short (overcharging) | Running V > 14.8 V | `charge_high` |
| Weak battery | Cranking sag deepens | `crank_sag` |
| Parasitic drain | Resting V decay steepens | `rest_rate` |
| Aging battery | Resting V trend downward over months | `rest_trend` |
| Connector heat | Intermittent running V drop | `charge_stability` |
| Ground corrosion | Running V lower than expected | `charge_delta` |

---

## 2. Architecture

Three layers, built in order:

- **Analysis** — `bikemate/dynatune.py`. Pure Python, no GUI, no I/O.
  `analyse(rows, live_config=None, mode="ride")` → nested dict of
  section → check → `{status, metric, detail}`.
- **History** — `bikemate/history.py`. Reads/writes `~/.bikemate/`.
  Pure Python, no GUI.
- **Presentation** — `bikemate/dynatune_window.py`,
  `bikemate/settings_history_window.py`. Tk.

Statuses: `PASS` / `WARN` / `FAIL` / `IDLE`. IDLE means "no evidence
either way" — not a failure.

---

## 3. Checks — `ride` section

Ride CSV rows: `epoch, lat, lon, volt, temp, state`.

| Check | Measurement | Rule | Healthy baseline |
|---|---|---|---|
| `duration` | row count vs time span | expected rows ≈ span / 5 s; gap > 2× → WARN | matches |
| `charge_delta` | avg running V − pre-ride resting V | ≥ 1.3 V → PASS; ≥ 1.0 V → WARN; < 1.0 V → FAIL | 14.2 − 12.9 = 1.3 V |
| `charge_high` | max running V | > 14.8 V sustained > 5 s → FAIL | ≤ 14.4 V |
| `charge_stability` | running V std dev over ride | > 0.3 V → WARN | ≤ 0.15 V |
| `crank_sag` | min V in first 30 s | ≥ 10.5 V → PASS; 9.5–10.5 → WARN; < 9.5 → FAIL | ~10.5–11 V observed |
| `sag_recovery` | seconds from min V back to running V | ≤ 3 s → PASS; ≤ 8 s → WARN | < 2 s |
| `under_duration` | seconds below `runUnder_mv` | 0 → PASS; ≤ 30 → WARN; > 30 → FAIL | 0 |
| `over_duration` | seconds above `runOver_mv` | 0 → PASS; ≤ 10 → WARN; > 10 → FAIL | 0 |
| `temp_range` | min/max temp | within plausible range (climate-adjusted) | 25–35 °C |
| `temp_trend` | temp slope over ride | rising fast → WARN | gentle |
| `end_clean` | last row V vs `runningExit_mv` | clean stop | yes |

## 4. Checks — `data` section

Wake CSV rows: `epoch, lat, lon, volt, temp, state, flags, sats`.

| Check | Measurement | Rule |
|---|---|---|
| `boot_count` | wake rows per 24 h | ≤ expected → PASS; > 2× → WARN |
| `clock_validity` | `no epoch` wake rows / total | 0 → PASS; ≤ 5% → WARN; > 5% → FAIL |
| `upload_success` | UPLOAD rows in wake log | at least 1 OK/day; any FAIL → FAIL |
| `upload_latency` | time from seal to OK | ≤ 5 min → PASS; > 30 min → WARN |
| `gps_fix_rate` | fix=1 rows / total | context-dependent |
| `wake_cadence` | gap between wake rows | > 2× expected → WARN; > 5× → FAIL |

## 5. Checks — `device` section

| Check | Measurement | Rule |
|---|---|---|
| `panic_count` | rows with `FLAG_PANIC` | 0 → PASS; any → FAIL |
| `under_flag` | rows with `FLAG_UNDER_VOLT` | 0 → PASS; any → WARN |
| `over_flag` | rows with `FLAG_OVER_VOLT` | 0 → PASS; any → WARN |
| `storage_usage` | LittleFS % used | < 75% → PASS; < 90% → WARN; ≥ 90% → FAIL |
| `maint_sessions` | maint start/end pairs | 0 → IDLE; ≥ 5/week → WARN |
| `sleep_balance` | SLEEP vs WAKE events | balanced → PASS |

## 6. Checks — `hardware` section

| Check | Measurement | Rule |
|---|---|---|
| `ntc_plausible` | temp range | 5–45 °C → PASS; edge → WARN |
| `divider_plausible` | resting V range | 12.0–13.2 V → PASS |
| `acc_transitions` | ACC on/off pairs in wake rows | balanced → PASS |
| `sensor_noise` | V std dev across adjacent wakes | ≤ 20 mV → PASS; > 50 mV → WARN |

## 7. Checks — `config` section

Operates on the current settings snapshot (from `/settings` over HTTP).

| Check | Rule |
|---|---|
| `run_order` | `runningEnter > runningExit` |
| `run_band` | `runUnder < runOver` |
| `monitor_order` | `monitorNormal > monitorWarning > monitorPanic` |
| `cross` | `monitorNormal ≤ runningExit` |
| `sag_floor` | `runUnder ≥ monitorNormal` |
| `charge_ceiling` | `runOver` between 14.5–15.0 V |
| `panic_floor` | `monitorPanic ≥ 11.8 V` |

## 8. Nice-to-have checks (v2+)

Require either additional hardware or long observation windows.

| Check | Needs | Value |
|---|---|---|
| `rest_rate` | 2+ resting readings days apart | mV/day decay rate |
| `rest_runway` | `rest_rate` + current resting V | days until 12.2 V |
| `rest_trend` | 3+ months of resting readings | months-level aging slope |
| `charge_curve` | Running V vs RPM | R/R health across rev band |
| `charge_regulation` | Run V over 10+ rides | R/R output stability |
| `temp_correlation` | Ride temp vs running V | R/R thermal behaviour |
| `crank_trend` | Crank sag over 10+ cold starts | Battery aging |
| `parasitic_drain_estimate` | Known Ah + rest_rate | average mA draw |
| `climate_compensation` | Weather API or seasonal baselines | Widen thresholds in cold |

---

## 9. Settings History

### 9.1 Data files

    ~/.bikemate/
      settings_history.json
      recommendations.json

### 9.2 Change entry

    {
      "ts": 1791445000,
      "key": "monitorWarning",
      "old": 12.4,
      "new": 12.5,
      "source": "manual" | "recommended",
      "rec_id": "r_..." | null,
      "transport": "HTTP" | "BLE",
      "note": ""
    }

### 9.3 Recommendation entry

    {
      "id": "r_20261008_120000_charge_delta",
      "created": 1791445000,
      "source_check": "charge_delta",
      "target_setting": "monitorWarning",
      "current_value": 12.4,
      "suggested_value": 12.5,
      "reason": "avg charge deficit 120 mV over 3 rides",
      "status": "proposed",
      "validated_at": null,
      "result": null
    }

Status lifecycle: `proposed` → `accepted` / `rejected` → `validated` /
`regressed` / `neutral`.

### 9.4 Where changes are logged

- `bikemate/app.py`, `apply()` HTTP branch — after `200 OK`
- `bikemate/app.py`, `apply()` BLE branch — after ACK `0x01`
- Firmware untouched; the log is a GUI artifact

### 9.5 Validation

Manual for v1. User selects an `accepted` recommendation → clicks
**Validate**. The GUI:

1. Runs `analyse()` over rides since the recommendation's change entry
   timestamp
2. Compares `source_check` metric before vs after
3. Writes `validated` / `regressed` / `neutral` back

Auto-validation on GUI start is v2.

---

## 10. Firmware-side companions (separate work)

### 10.1 Charge-failure email alert

The prime directive demands advance warning before stranding. The
GUI-side `charge_delta` check catches failure after it happens, on the
next GUI sync. Firmware can catch it during the ride that detected it.

- On ride close, compute running V average
- If avg < `runUnder_mv` for > 30 s during the ride → set `chargeFail`
  RTC flag
- On next wake with WiFi available, send alert email

### 10.2 Rest-rate RTC log

- Every wake without engine running: record `(epoch, resting_v)` into a
  rolling 7-entry buffer
- On upload cycle, flush buffer to a sidecar file
- GUI reads sidecar to compute mV/day trend

---

## 11. Build order

| # | Deliverable | Depends on | Verify |
|---|---|---|---|
| 1 | `history.py` — logging only | — | Change setting, restart GUI, see entry |
| 2 | `settings_history_window.py` | 1 | Window shows current + changes |
| 3 | `dynatune.py` — checks, no recs | — | Run `analyse()`, see grid |
| 4 | `dynatune_window.py` | 3 | Window renders sections |
| 5 | Recommendations in `dynatune.py` | 3 | Trigger FAIL, see rec |
| 6 | Rec → change linkage | 1, 5 | Apply rec, see `rec_id` in history |
| 7 | Manual validate | 6 | Change setting, ride, validate |
| 8 | Firmware `chargeFail` + mail | — | Simulated ride, next wake sends mail |
| 9 | Firmware rest-rate RTC log | — | Two days apart, sidecar has two rows |

Steps 1, 3, 8, 9 independent. Steps 2, 4, 5 depend on foundations.
Steps 6, 7 depend on both branches.

---

## 12. Out of scope for v1

- Firmware writes `dyna_*` to `DYNA/`
- Auto-run at ride-close
- Auto-validate on GUI start
- Fleet / multi-bike
- Export / import of history JSON
- ML / anomaly detection
- OBD-II integration
- Any change to flash layout, NVS schema, or upload path
- Any new HTTP endpoint or BLE characteristic
- Automatic application of recommendations

---

## 13. Open questions

1. **Source of `live_config` offline** — read from bike during maint and
   cache to `~/.bikemate/live_config.json`; or mirror NVS on every
   `/settings` read.
2. **Which ride Dyna Tune analyses by default** — in-memory
   `state.latest_ride` or newest file in `~/bike-mate-drive/rides/`.
3. **v1 recommendation list** — the checks in section 7 each imply a
   suggestion rule; confirm or trim.
4. **Climate context** — pass current weather into Dyna Tune for
   threshold adjustment, or keep climate baked into seasonal baselines.
5. **Where Settings History lives** — Reports menu vs Maintenance.
6. **Firmware `chargeFail` timing** — mail mid-ride (WiFi during engine
   running, battery cost) or next wake (safer, one wake delay).
