# Bike-Mate — V2.00

Motorcycle battery, temperature and ride logger. ESP32C3 + BLE + OLED.
This is the **modularised** V2.00 release — behaviour identical to the
V1.82 monolith, but split across files so each module can be worked on
in isolation.

---

## Version history

| Version | Notes |
|---------|-------|
| V1.0–V1.55 | Dashboard, cards, graphs, BLE pull, ride CSVs (GUI side) |
| V1.56 | AGM battery colours, empty-notify guard |
| V1.73 | De-pinked, cleaned status label, banner default, Ride Logs rewrite |
| V1.75 | Parse 32-byte `RideSummary` with `preRideVolt` field |
| V1.76 | Show cold voltage on Last Ride card and Ride Logs cards |
| V1.81 | `BENCH_MODE` no longer disables sleep — sleep runs in all modes |
| V1.82 | Last monolithic baseline (known-good, verified on hardware) |
| **V2.00** | **Modularised. Behaviour identical to V1.82. Verified compile + upload + boot + BLE on SuperMini C3.** |

---

## Hardware

- **MCU:** ESP32-C3 SuperMini (with onboard 72×40 OLED)
- **Display:** SSD1306-compatible 72×40 panel, I²C on SDA=5, SCL=6
- **Voltage sense:** resistor divider R_TOP 100k / R_BOTTOM 10k on GPIO 0
- **Thermistor:** 10k NTC on GPIO 3
- **ACC LED:** GPIO 1
- **Buzzer:** GPIO 4 (LEDC channel 0, 2 kHz)
- **Status LED:** GPIO 10
- **Framework:** Arduino ESP32 core 2.x (2.0.17), ESP32C3 Dev Module

---

## Features (V2.00)

### Voltage monitoring
- 5-sample median-of-ADC on the voltage divider
- Jump filter (rejects >3 V step from last reading)
- Plausibility window 8.0–20.0 V
- Linear calibration: `V = (raw × 0.00073643) − 0.01602`, ×11 divider ratio

### Temperature monitoring
- 10k NTC thermistor with 3950 K Beta model
- Clamped voltage input, logarithm-based conversion to °C
- Sampled alongside every voltage read

### State machine
- **MONITOR** — idle, safe to sleep
- **RUNNING** — engine detected (V ≥ 13.8 V)
- **PANIC** — low battery (V < 12.0 V), 5× low-batt beep, re-triggers every 120 s
- **Ride countdown** — 30 s settle time after engine start, then ACC ON
- **Park arming** — 10 s park detection, then 10 s arming countdown
- Transitions logged to serial as `[STATE] ...`

### Ride logging (NVS)
- 10 ride slots in non-volatile storage
- One row per 30 s: `{epoch, volt_x100, temp, state}` (8 bytes)
- Up to 512 rows per ride
- On close: summary struct written alongside rows
  - Duration, min/max/avg voltage, min/max temperature
  - Under/over-voltage seconds
  - Pre-ride (cold) voltage
  - Flags: UNDER_VOLT, OVER_VOLT, FREEZING, HOT_AMBIENT, ENCLOSURE_HOT
- Zero-duration rides auto-discarded
- Invalid-clock rides auto-discarded

### BLE
- Device name `Bike-Mate`
- Service UUID `4fafc201-...-c5c9c331914b`
- **DATA** characteristic — JSON status notify every 2 s
  - Fields: `v` (voltage), `t` (temp), `a` (ACC), `e` (engine), `w` (warning), `s` (state), `p` (panic)
- **TIME** characteristic — GUI writes 4-byte little-endian epoch on connect
- **STREAM** characteristic — notify, ride summary + row chunks (16 bytes per notify, 2 rows)
- **REQUEST** characteristic — GUI writes 4-byte highest-known epoch; firmware replies 0x00 (nothing), 0x01 (pushing), 0x02 (done)
- MTU 185
- Advertising restarts every 3 s while disconnected
- On client disconnect: advertising restarts automatically

### OLED (72×40)
- Splash screen on cold boot, 3 s
- Edge-triggered redraw (only updates when a rendered value changes)
- Four display modes: PANIC, countdown, arming countdown, riding/monitor
- Uses `Adafruit_SSD1306_72x40` library

### Sleep / wake
- **BENCH_MODE 1** — always 30 s wake interval
- **BENCH_MODE 0** — 300 s day (04:00–22:00), 600 s night (22:00–04:00)
- Deep sleep only when idle: no PANIC, no engine, no ACC, no countdowns, not logging, no alarms
- Before sleep: 3 s advertising window, then 5 s opportunity for GUI push request
- RTC-persistent state survives sleep: `macTimeEpoch`, `totalSeconds`, `engineWasRunning`, `accState`, `inPanic`, ride stats, ride buffer
- Cold boot resets: time, all ride state, resting voltage

### Buzzer / status LED
- Two-tone alarm sequence: 150 ms on, 250 ms gap, 150 ms on
- Low-battery beep sequence: 5 × 150 ms beeps, 150 ms gaps
- Status LED mirrors buzzer state (GPIO 10)
- Wake flash on boot: 150 ms on, 250 ms off, 150 ms on

---

## File layout

| File | Purpose |
|------|---------|
| `bike_mate.ino` | Coordinator: `setup()`, `loop()`, state machine, sleep, time helpers, `tprint()` |
| `Config.h` | All `#define`s, pin numbers, UUIDs, thresholds, struct definitions |
| `Sensors.h/.cpp` | ADC reads for voltage and thermistor |
| `Buzzer.h/.cpp` | Buzzer tone sequences + status LED |
| `RideLogger.h/.cpp` | NVS ride logging, RTC-persistent ride state |
| `BleManager.h/.cpp` | BLE server, callbacks, notify, ride push |
| `DisplayManager.h/.cpp` | OLED rendering, edge-triggered updates |

---

## Build & upload

1. Arduino IDE 2.x
2. Board: **ESP32C3 Dev Module**
3. ESP32 core version: **2.0.17** (2.x required — V3.x is not supported)
4. Libraries:
   - Adafruit GFX
   - Adafruit BusIO
   - Adafruit SSD1306
   - **Adafruit_SSD1306_72x40** (custom — required for the 72×40 panel)
5. Sketch folder: `~/Documents/Arduino/bike_mate/`
6. Open `bike_mate.ino`, compile, upload

---

## Protected systems (do not casually modify)

- **OLED** — pins, library, init sequence, draw timing, edge-trigger logic
- **Sleep/wake** — wake intervals, RTC state, cold-boot reset, sleep gate conditions
- **BLE** — UUIDs, advertising cadence, callback behaviour, notify timing, push protocol

Any change to these must be tested end-to-end on real hardware.

---

## Roadmap

| Version | Focus |
|---------|-------|
| **V2.50** | Fake voltage source for PSU-less testing of state machine + logging |
| **V3.00** | Perfboard + external OLED — first hardware-driven pin/display changes |
| Post-V3 | GPS module, post-ride processing (GPX), runtime sleep mode switch |

---

## Known deviations from V1.82

**None.** V2.00 is a pure structural port. Behaviour is byte-for-byte identical.

One intentional difference during development only:
- `.ino` file was previously named `bike_mate.ino` (V1.82), still is (V2.00). No change.

---

## GUI companion

- **GUI version:** V1.76
- **Wire format:** compatible (32-byte `RideSummary`, 8-byte `RideRow`)
- **Location:** `~/Documents/Arduino/bike_mate/bikemate.py` (also on GitHub)
- **Requires:** `bleak` for BLE, Python 3.8+

GUI version string is stale (V1.76) but wire format matches V2.00 firmware.
