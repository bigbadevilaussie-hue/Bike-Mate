# Bike-Mate — Dyna Tune

Post-ride diagnostics and Settings History. Two connected features:

- **Dyna Tune** — KPI board that analyses a ride log (and optionally
  wake logs) against a fixed set of checks, producing a grid of
  PASS / WARN / FAIL / IDLE results plus recommendations.
- **Settings History** — a hypothesis register. Every settings change
  is logged with the reason it was made. When the reason was a Dyna Tune
  recommendation, the change is tracked to see whether it actually
  improved the target KPI.

The closed loop: `analyse → recommend → apply → measure → validate or
revert`.

The prime directive is unchanged — do not drain the bike battery past
crank. Dyna Tune is how the trend gets seen before the cliff.

## Status

Specification v2. `bikemate/dynatune_window.py` exists as a mock window.
`bikemate/dynatune.py` (the analysis engine) is not yet written.

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

**Rest-rate note:** the 12.9 → 12.6 V drop over 7 days is not all
parasitic drain. The first few hours after engine-off are surface charge
bleed-off, not real current. A meaningful parasitic-drain estimate needs
readings taken at least 4–6 h after engine-off, and preferably overnight.
Early drafts of this spec computed ~2 mA draw from the 7-day figure;
that number is wrong. The real drain is much lower, and the check that
uses it (`rest_rate`) requires the corrected method to be meaningful.

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
  reads lower. Cranking sag deeper. Thresholds need seasonal baselines
  so a cold morning doesn't false-WARN.
- **Summer:** heat soak in engine bay. R/R output drops slightly.
  Resting V reads artificially low for 30–60 min after a hot ride.
- **Humidity:** electronics need conformal coating or sealed enclosure.

### 1.5 Failure-to-diagnostic mapping

| Failure mode | First observable symptom | Diagnostic check |
|---|---|---|
| R/R open (no charging) | Running V = resting V | `charge_delta` |
| R/R short (overcharging) | Running V > 14.8 V | `charge_high` |
| Weak battery | Cranking sag deepens | `crank_sag` |
| Parasitic drain | Resting V decay steepens | `rest_rate` (v2) |
| Aging battery | Resting V trend downward over months | `rest_trend` (v2) |
| Connector heat | Intermittent running V drop | `charge_dropout` |
| Ground corrosion | Running V lower than expected | `charge_delta` |

---

## 2. Architecture

Three layers, built in order:

- **Analysis** — `bikemate/dynatune.py`. Pure Python, no GUI, no I/O.
  `analyse(rows, live_config=None, mode="ride")` → nested dict of
  section → check → `{status, metric, detail}`.
- **History** — `bikemate/history.py`. Reads/writes `~/.bikemate/`.
  Pure Python, no GUI.
- **Presentation** — `bikemate/dynatune_window.py` (exists as a mock),
  `bikemate/settings_history_window.py` (to be built).

Statuses: `PASS` / `WARN` / `FAIL` / `IDLE`. IDLE means "no evidence
either way" — not a failure.

**Read-only by design.** Dyna Tune never changes bike settings. It
analyses and recommends. Application of a recommendation is a user
action through the existing Settings dialog, and that action is logged
by the History layer.

**Hard boundary.** `dynatune.py` takes rows and config, returns
structured results. No filesystem, no HTTP/BLE, no Tk. That makes it
unit-testable against synthetic ride CSVs.

---

## 3. Checks — `ride` section (v1)

Ride CSV rows: `epoch, lat, lon, volt, temp, state`.

### 3.1 `charge_delta` — the primary charging check

The critical check that would have caught the R/R failure. Two signals:

- **Primary: absolute average running V.**
  - PASS: ≥ 13.8 V
  - WARN: 13.4–13.8 V
  - FAIL: < 13.4 V
- **Secondary: delta (avg running V − pre-ride resting V).**
  - PASS: ≥ 1.3 V
  - WARN: 1.0–1.3 V
  - FAIL: < 1.0 V

The primary signal catches the failure mode directly. The secondary
signal is context: a healthy bike after a long sit has low pre-ride
resting V (surface charge dissipated), so a healthy delta reads higher
than on a recently-charged battery. Using delta alone produces false
WARNs; using absolute V alone misses subtle degradation. Together they
cover both.

Additional sub-metrics for the detail popup, not separately
status-scored:

- `charge_min` — lowest running V
- `charge_max` — highest running V
- `charge_range` — charge_max − charge_min

### 3.2 `charge_high`

Max sustained running V over any 5 s window.

- PASS: < 14.6 V
- WARN: 14.6–14.8 V
- FAIL: > 14.8 V sustained > 5 s

**Overcharge is a hardware fault, not a config problem.** A FAIL here
produces an informational recommendation ("investigate regulator"), never
a "raise `runOver`" recommendation.

### 3.3 `crank_sag`

Minimum V in the first 30 s of the ride.

- Rolling baseline: median of the last 10 cold starts (once that history
  exists). PASS if within 0.5 V of the median.
- Fixed backstop until the baseline exists:
  - PASS: ≥ 10.5 V
  - WARN: 9.5–10.5 V
  - FAIL: < 9.5 V

Temperature compensation: subtract 3 mV/°C per cell (≈ 20 mV/°C on a
12 V battery) for readings below 20 °C. Prevents winter false WARNs.

### 3.4 `sag_recovery`

Seconds from min V during cranking back to running V (within 0.2 V of
the ride's average running V).

- PASS: ≤ 3 s
- WARN: 3–8 s
- FAIL: > 8 s

### 3.5 `charge_stability`

Standard deviation of running V over the ride.

- PASS: ≤ 0.15 V
- WARN: 0.15–0.30 V
- FAIL: > 0.30 V

Companion to `charge_dropout` (see v2 below). Stability catches
sustained noise; dropout catches brief dips.

### 3.6 `under_duration`

Seconds below `runUnder_mv`.

- PASS: 0
- WARN: 1–30 s
- FAIL: > 30 s

### 3.7 `over_duration`

Seconds above `runOver_mv`.

- PASS: 0
- WARN: 1–10 s
- FAIL: > 10 s

### 3.8 `end_clean`

Comparison of last row V against `runningExit_mv`.

- PASS: last V ≤ `runningExit_mv` + 0.3 V (ride ended as engine stopped)
- WARN: last V within 0.5 V of `runningEnter_mv` (ambiguous)
- FAIL: last V above `runningEnter_mv` (ride ended mid-run)

### 3.9 `min_data`

Sanity gate for the whole `ride` section. If any of these is true,
every check in the section returns IDLE:

- Row count < 10
- Time span < 60 s
- Fewer than 5 rows with a parseable voltage
- Any clock discontinuity > 30 s between consecutive rows

Prevents a 30 s ride from generating false PASS or FAIL results.

### 3.10 Pre-ride resting V source

`charge_delta` needs a reliable pre-ride resting V. Options, in order
of preference:

1. Last resting row in the wake log before the ride start epoch, if it
   was recorded ≥ 30 min before the ride began.
2. First row of the ride minus 0.3 V (assumes surface-charge
   dissipation at ride start).
3. The ride summary's `preRideVolt` field, if present.

If none are available, `charge_delta` returns IDLE with a note.

---

## 4. Checks — `data` section (v1, wake log)

Wake CSV rows: `epoch, lat, lon, volt, temp, state, flags, sats`.

| Check | Measurement | Rule |
|---|---|---|
| `clock_validity` | `no epoch` wake rows / total | 0 → PASS; ≤ 5% → WARN; > 5% → FAIL |
| `upload_success` | UPLOAD rows | at least 1 OK/day; any FAIL → FAIL |

Other checks in this section (`boot_count`, `upload_latency`,
`wake_cadence`, `gps_fix_rate`) render as IDLE in v1 — rules stubbed,
not enforced.

---

## 5. Checks — `device` section (v1)

| Check | Measurement | Rule |
|---|---|---|
| `panic_count` | rows with `FLAG_PANIC` | 0 → PASS; any → FAIL |
| `storage_usage` | LittleFS % used | < 75% → PASS; < 90% → WARN; ≥ 90% → FAIL |

`under_flag`, `over_flag`, `sleep_balance`, `maint_sessions` render
IDLE in v1.

---

## 6. Checks — `hardware` section (v1)

| Check | Measurement | Rule |
|---|---|---|
| `ntc_plausible` | temp range | 5–45 °C → PASS; edge → WARN; outside → FAIL |
| `divider_plausible` | resting V range | 12.0–13.2 V → PASS |

`acc_transitions`, `sensor_noise` render IDLE in v1.

**`ntc_plausible` climate note:** 45 °C upper bound is fine for the
sensor itself but an enclosure in engine-bay heat or direct sun after a
hot ride can exceed that legitimately. Threshold is a starting point;
widen after observing summer readings.

---

## 7. Checks — `config` section (v1)

Operates on the current settings snapshot (from `/settings` over HTTP,
cached to `~/.bikemate/live_config.json`).

| Check | Rule |
|---|---|
| `run_order` | `runningEnter > runningExit` |
| `run_band` | `runUnder < runOver` |
| `monitor_order` | `monitorNormal > monitorWarning > monitorPanic` |
| `cross` | `monitorNormal ≤ runningExit` |
| `sag_floor` | `runUnder ≥ monitorNormal` |
| `charge_ceiling` | `runOver` between 14.5–15.0 V |
| `panic_floor` | `monitorPanic ≥ 11.8 V` |

---

## 8. Deferred checks (v2+)

Require either additional hardware, long observation windows, or
firmware-side data that doesn't exist yet.

| Check | Needs |
|---|---|
| `charge_dropout` | Second-by-second V tracking (current spec is 5 s cadence) |
| `rest_rate` | Resting readings ≥ 4–6 h post-engine-off, rolling buffer |
| `rest_runway` | `rest_rate` + current resting V |
| `rest_trend` | 3+ months of resting readings |
| `charge_curve` | Running V vs RPM |
| `charge_regulation` | Run V over 10+ rides |
| `temp_correlation` | Ride temp vs running V |
| `crank_trend` | Crank sag over 10+ cold starts |
| `parasitic_drain_estimate` | Known Ah + corrected `rest_rate` |
| `climate_compensation` | Seasonal baselines in v1, live weather API v2 |
| `boot_count` | Rule needs definition |
| `upload_latency` | Rule needs definition |
| `wake_cadence` | Rule needs definition |
| `gps_fix_rate` | Rule needs definition |
| `under_flag`, `over_flag` | Flag bits in wake rows need verifying |
| `sleep_balance` | Rule needs definition |
| `maint_sessions` | Rule needs definition |
| `acc_transitions` | Rule needs definition |
| `sensor_noise` | Rule needs definition |

---

## 9. Recommendations

Recommendations are produced only for checks where the fix is a
**settings change**, and only when there is enough evidence.

### 9.1 v1 recommendation list

| Source check | Recommendation | Notes |
|---|---|---|
| `charge_delta` FAIL | Investigate charging system (no setting change) | Hardware fault |
| `charge_high` FAIL | Investigate regulator (no setting change) | Hardware fault |
| `under_duration` FAIL | Consider raising `runUnder_mv` | Only if repeated over 4+ rides |
| `over_duration` FAIL | Consider raising `runOver_mv` | Only if repeated over 4+ rides |
| `config` any FAIL | Fix the specific setting | Immediate, no evidence wait |
| `crank_sag` WARN/FAIL | Note battery age / CCA concern (informational) | No setting change |

### 9.2 Confidence levels

Every recommendation carries a confidence based on evidence count:

- **observation** — 1 ride. Displayed but not actionable.
- **weak** — 2–3 rides. Suggest manual review.
- **strong** — 4+ consistent rides. Actionable, has a Validate button.

A single weird ride cannot generate a strong recommendation.

### 9.3 Persistence before FAIL

For any check except `charge_high`, a single bad ride produces WARN,
not FAIL. FAIL requires the condition to hold in 2 of the last 3 rides.

`charge_high` fails immediately — overcharge damages the battery
quickly and cannot wait for confirmation.

### 9.4 Recommendation schema

```json
{
  "id": "r_20261008_120000_charge_delta",
  "confidence": "strong",
  "evidence_rides": [1791445000, 1791448600, 1791452200, 1791455800],
  "created": 1791445000,
  "source_check": "charge_delta",
  "target_setting": null,
  "current_value": null,
  "suggested_value": null,
  "reason": "avg running V = 13.42 over 4 rides, was 14.18 previous week",
  "why": {
    "avg_running": 13.42,
    "pre_ride_resting": 12.91,
    "delta": 0.51,
    "observed_over": 4
  },
  "status": "proposed",
  "validated_at": null,
  "result": null
}
The why field carries the numbers behind the recommendation. The
detail popup in the window reads them directly — no recomputation, no
hidden math.
9.5 Recommendation lifecycle

proposed → accepted / rejected → validated / regressed /
neutral.

    proposed — produced by analyse(), shown in the Dyna Tune window.

    accepted — user applied the recommendation through the Settings
    dialog. Recorded in settings_history.json with rec_id linking back.

    rejected — user dismissed it. Stored for posterity.

    validated — after ≥ 3 post-change rides in comparable conditions
    (similar temp band, similar ride length), the target KPI improved.

    regressed — the KPI got worse.

    neutral — no measurable change.

9.6 Validation UX

When the user clicks Validate, the window shows:

    Before / after metric values

    Rides used

    Temperature and ride-length comparison (to show conditions matched)

    A one-click Revert button on regressed entries

Revert logs a new change entry with reverted_from set to the original
change's id. The bike's settings are written back through the normal
Settings apply path.
~/.bikemate/
  settings_history.json     schema: 1
  recommendations.json      schema: 1
  live_config.json          schema: 1
  baselines.json            schema: 1
  last_good.json            schema: 1
9.5 Recommendation lifecycle

proposed → accepted / rejected → validated / regressed /
neutral.

    proposed — produced by analyse(), shown in the Dyna Tune window.

    accepted — user applied the recommendation through the Settings
    dialog. Recorded in settings_history.json with rec_id linking back.

    rejected — user dismissed it. Stored for posterity.

    validated — after ≥ 3 post-change rides in comparable conditions
    (similar temp band, similar ride length), the target KPI improved.

    regressed — the KPI got worse.

    neutral — no measurable change.

9.6 Validation UX

When the user clicks Validate, the window shows:

    Before / after metric values

    Rides used

    Temperature and ride-length comparison (to show conditions matched)

    A one-click Revert button on regressed entries

Revert logs a new change entry with reverted_from set to the original
change's id. The bike's settings are written back through the normal
Settings apply path.
10. Settings History
10.1 Data files
text

~/.bikemate/
  settings_history.json     schema: 1
  recommendations.json      schema: 1
  live_config.json          schema: 1
  baselines.json            schema: 1
  last_good.json            schema: 1

Every JSON file has a schema field. Bumping it later requires a
migration path.
10.2 Change entry
json

{
  "schema": 1,
  "bike_id": "sprint-st-1050",
  "ts": 1791445000,
  "key": "monitorWarning",
  "old": 12.4,
  "new": 12.5,
  "source": "manual" | "recommended",
  "rec_id": "r_..." | null,
  "reverted_from": "s_..." | null,
  "transport": "HTTP" | "BLE",
  "note": ""
}

bike_id is a short string so multi-bike is not a migration later. v1
only ever has one value.
10.3 Where changes are logged

    bikemate/app.py, apply() HTTP branch — after 200 OK

    bikemate/app.py, apply() BLE branch — after ACK 0x01

    Firmware untouched; the log is a GUI artifact

10.4 Config snapshot per ride

Every ride summary stored in NVS gains a compact config snapshot:
the seven settings values active at ride start. This kills the
live_config open question — "what settings were active for this
ride" is never a guess.

Implementation: add a cfg blob to the RideSummary struct or write
a sidecar. Firmware change required. Tracked as v2 firmware work.
10.5 Default ride selection

Dyna Tune analyses the newest file in ~/bike-mate-drive/rides/
by default, with a picker for older files. Not state.latest_ride
from memory — that disappears when the app closes and is harder to
unit-test.
11. Firmware-side companions (deferred)
11.1 Charge-failure email alert

The prime directive demands advance warning before stranding. The
GUI-side charge_delta check catches failure after it happens, on the
next GUI sync. Firmware can catch it during the ride that detected it.

    On ride close, compute running V average

    If avg < runUnder_mv for > 30 s during the ride → set chargeFail
    RTC flag

    On next wake with WiFi available, send alert email

    Next-wake mail, not mid-ride. A mid-ride WiFi cost isn't worth
    saving one wake cycle.

11.2 Rest-rate RTC log

    Every wake without engine running: record (epoch, resting_v) into a
    rolling 7-entry buffer

    On upload cycle, flush buffer to a sidecar file

    GUI reads sidecar to compute mV/day trend

Both deferred until the PC-side Dyna Tune and History are demonstrated
against existing CSVs.
12. Build order
#	Deliverable	Depends on	Verify
1	bikemate/history.py — logging only	—	Change setting, restart GUI, see entry
2	bikemate/settings_history_window.py	1	Window shows current + changes
3	bikemate/dynatune.py — ride section only	—	Run on real ride CSV, see results
4	Wire open_dynatune to real analyse()	3	Window shows real ride data
5	dynatune.py — data / device / hardware / config sections	3	Full grid populates
6	Recommendations in dynatune.py	3	Trigger a FAIL, see rec
7	Rec → change linkage	1, 6	Apply rec, see rec_id in history
8	Manual validate + revert	7	Change setting, ride, validate
9	Firmware chargeFail + mail	—	Simulated ride, next wake sends mail
10	Firmware rest-rate RTC log	—	Two days apart, sidecar has two rows

Steps 1, 3, 9, 10 independent. Steps 2, 4-8 depend on foundations.

Mock state today: dynatune_window.py exists with placeholder
analysis from open_dynatune(). Data is fake until step 3 and 4.
13. Out of scope for v1

    Firmware writes dyna_* to DYNA/

    Auto-run at ride-close

    Auto-validate on GUI start

    Fleet / multi-bike

    Export / import of history JSON

    ML / anomaly detection

    OBD-II integration

    Any change to flash layout, NVS schema, or upload path (except 10.4)

    Any new HTTP endpoint or BLE characteristic

    Automatic application of recommendations

    Live weather API (seasonal baselines first)

14. Open questions

    live_config offline — resolve by caching on every /settings
    read to ~/.bikemate/live_config.json with a fetched_at
    timestamp. Stale config (> 24 h) marks the config section WARN.

    Default ride — resolved: newest file on disk (section 10.5).

    v1 recommendation list — trimmed in section 9.1.

    Climate — seasonal baselines baked in for v1; live weather API
    deferred to v2.

    Where Settings History lives — Maintenance menu (operational,
    not a report).

    Firmware chargeFail timing — resolved: next wake
    (section 11.1).

15. Review history

Three external reviews of the v1 spec landed 2026-10-08. Their
suggestions are folded into this document.

    Claude — flagged rest-rate math error, charge_delta baseline
    problem, temperature compensation, rolling baselines, persistence
    rules, charge_dropout, validation evidence requirements.

    ChatGPT — read-only design, dynatune.py hard boundary, trimmed
    recommendation surface, confidence levels, "why" field on
    recommendations, firmware companions later.

    Grok — pre-ride resting V source, tight recommendation list,
    seasonal baselines, disk-file default ride, bike_id, schema
    versioning, validation UX.

Where the three diverged on charge_delta, the resolution is
absolute-primary + delta-secondary + companions (section 3.1). Where
they diverged on temperature, seasonal baselines ship in v1 and live
compensation is deferred (section 1.4, section 8).
