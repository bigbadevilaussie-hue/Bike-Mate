# Bike-Mate — Bench Setup

Physical snapshot of the breadboard at firmware V4.36.
Update this file whenever the wiring changes. If it drifts from reality,
reality wins — fix this file.

Snapshot date: 2026-10-01
Firmware on bench: V4.36

---

## POWER ARCHITECTURE (bench)

Two independent power sources:

    USB-C (Mac) ──── ESP32-C3 SuperMini VIN
                         │
                         └── onboard LDO → 3.3V rail → ESP32, OLED

    PSU (30V/3A) ──── 12V rail ──── voltage divider ──── GPIO 0
                               └─── buzzer red wire

- USB-C powers the ESP32 during development
- PSU (currently OFF) powers the 12V rail for the divider and buzzer
- TVS not fitted
- MP1584EN buck wired but not used (ESP32 runs off USB)

## POWER ARCHITECTURE (target board)

    12V battery
        │
        └── [12V rocker switch]
                │
                ├── SA16CA TVS → GND
                │
                ├── MP1584EN buck ── 5V ── ESP32 VIN ── LDO ── 3.3V rail
                │                                                  │
                │                                                  ├── OLED
                │                                                  ├── NTC divider
                │                                                  └── ESP32
                │
                ├── voltage divider ── GPIO 0
                │
                └── PN2222A ── IRF4905 ── switched 12V rail
                                              ├── GPS
                                              └── USB charger

---

## TRACED CIRCUITS (V4.36 bench)

### Section 1 — Voltage divider (GPIO 0)

    12V ──[98.8k]──┬──[9.98k]── GND
                   │
                   ├──[100nF]── GND
                   │
                   └── GPIO 0

- Top resistor: 98.8kΩ measured
- Bottom resistor: 9.98kΩ measured
- Actual ratio: 9.90:1
- Firmware assumes 10:1 (`BATTERY_SLOPE 0.008058`) — ~1% calibration error
- 100nF ceramic fitted (anti-noise + ADC reservoir)

### Section 2 — NTC (GPIO 3)

    3.3V ──[NTC MF52AT 10k B3950]──┬──[9.7k]── GND
                                   │
                                   └── GPIO 3

- NTC: MF52AT 10k, Beta **3950** (firmware currently uses 4600 — wrong)
- Series: 9.7kΩ measured (firmware uses 10000 — wrong)
- `-2.3f` offset in firmware was tuned for old NTC — needs removal/recalibration
- **No filter cap on this node** — cabin temp only, cosmetic. Add 100nF if noisy.

### Section 3 — OLED (I2C)

    ESP32 3.3V ── OLED VCC
    ESP32 GND  ── OLED GND
    GPIO 6     ── OLED SDA
    GPIO 9     ── OLED SCL

- SSD1306 128x64, I2C addr 0x3C
- GPIO 9 is an ESP32-C3 boot strap pin — do not add strong external pull-downs
- OLED module has onboard I2C pull-ups

### Section 4 — Buzzer (GPIO 4)

    12V ── red wire ── 12V piezo buzzer ── black wire ── PN2222A collector (right leg)
                                                             │
                                                             B ──[1k]── GPIO 4
                                                             │
                                                             ├──[10k]── GND
                                                             │
                                                             E ── GND

- Buzzer: SFM-27-I, 12V piezo, 100dB continuous (active)
- PN2222A (flat face toward you, legs down): Left=E, Middle=B, Right=C
- Base drive: 1k series + 10k pulldown
- Base current: ~2.6mA when GPIO HIGH — well within limits
- **No flyback diode** — not needed, piezo is capacitive
- Firmware drives with LEDC 2kHz tone — works, though buzzer is active so tone is wasted

### Section 5 — ACC LED (GPIO 1)

    GPIO 1 ──[324Ω]── green LED ── GND

- ACC (accessories) indicator light
- Simulates the future switched 12V rail
- 324Ω measured (nominal 330Ω, within tolerance)
- Lights when `accState = true` (engine running, ride in progress)

### Section 6 — Switched 12V rail (DESIGN ONLY, not built)

    GPIO 1 ──┬── [324Ω] ── green LED ── GND
             │
             └── [1k] ── PN2222A base
                           │
                           ├──[10k]── GND
                           │
                           C ── IRF4905 gate
                           E ── GND

    12V ──┬── [10k pull-up] ──┬── IRF4905 gate
          │                    │
          └── IRF4905 source   │
                               │
                         IRF4905 drain ── switched 12V rail
                                            ├── GPS VCC
                                            └── USB charger VIN

- Same GPIO 1 drives both the dashboard LED and the MOSFET switch
- When GPIO 1 HIGH: LED lit AND switched rail live
- P-FET (IRF4905) high-side switch — off when GPIO LOW
- Load: GPS (~45mA) + USB charger (variable)
- No flyback diode needed — resistive/capacitive loads

### Section 7 — Status LED (GPIO 10)

    GPIO 10 ──[330Ω]── red LED ── GND

- Mirrors buzzer via `buzzerOn()` / `buzzerOff()` when awake
- `wakeFlash()` on every boot/wake = double-blink (150ms on, 250ms off, 150ms on)
- In sleep, GPIO 10 LOW between wakes
- Fake alarm flash: `wakeFlash()` on every timer wake appears as "beep-beep... pause... beep-beep" — reads as armed alarm from a distance

### Section 8 — Power input

Bench:

    USB-C → ESP32-C3 VIN → onboard LDO → 3.3V rail
    PSU   → 12V rail → divider + buzzer

Board target:

    12V battery → rocker switch → TVS → buck → 5V → ESP32 VIN
                                     → divider → GPIO 0
                                     → NPN + P-FET → switched 12V

---

## BOARD NOTES

### XCW ESP32-C3 SuperMini (current bench board)
- **WiFi issue:** requires TX power reduced to 8.5 dBm
  - `esp_wifi_set_max_tx_power(34);` in `WifiManager.cpp`
  - Default TX (~20dBm) causes connect failures
  - This is a known hardware quirk of this board revision
- No onboard OLED
- Flash chips: Micron 0x164020 (with OLED), XMC 0x164046 (no OLED)

### ESP32-C3 with 0.42" OLED (spare, unused)
- **WiFi works at default TX power** — no fix needed
- Onboard 0.42" OLED
- Different pin map from SuperMini
- Candidate for future board if WiFi range matters

---

## FIRMWARE STATE

- `BIKE_MATE_VERSION "4.36"`
- `BENCH_MODE 1` (30s wake) — do not deploy
- `GPS_STUB_ENABLED 1` (fake coordinates)
- NTC: reverted to V4.34 constants, reads within ~1.5C of room temp
- Deep sleep tested and stable
- BLE + GUI + time sync + ride pull working

### Sample log (PSU off, USB on)

    [ADC] raw=0 battery=12.60V ntc=2364 temp=28.8C
    [BOOT] ready
    [SENSOR] V=12.60 raw=0 ntc=2363 temp=28.8C
    [SLEEP] mode=BENCH V=12.60 conn=0 wake=30s

`raw=0` = PSU off. `battery=12.60V` = stale default (known bug).

---

## HARDWARE TODO

Ordered by dependency. Software TODO is in `TODO.md`.

### Bench (before perfboard)

- [ ] **Turn PSU on to 12.0V** — verify divider reads correctly, observe raw value
- [ ] **Calibrate `BATTERY_SLOPE`** — PSU at 12.0V, 12.6V, 13.8V, compare to DMM
- [ ] **Measure buck quiescent** — PSU 12V → buck, no load on 5V, DMM in series
- [ ] **Measure ESP32 sleep current** — PSU powering the system, DMM in series on 12V
- [ ] **Measure total board sleep current** — confirms prime directive compliance
- [ ] **Add 100nF to GPIO 3** (optional) — only if NTC reading is noisy
- [ ] **Wire TVS (SA16CA)** — 12V input to GND, cathode to 12V
- [ ] **Power ESP32 from buck** — disconnect USB, verify 5V rail works
- [ ] **Build Section 6** — IRF4905 + PN2222A switched 12V rail on breadboard
- [ ] **Wire GPS (GY-NEO6M)** — UART pins TBD, add to Config.h
- [ ] **Write GPS NMEA parser** — replace `GPS_STUB_ENABLED 1`
- [ ] **Wire USB charger (QC3.0 buck)** — on switched rail
- [ ] **Test switched rail** — verify GPS + USB only power when ACC on

### Perfboard (after breadboard is proven)

- [ ] **Decide board** — XCW SuperMini vs ESP32-C3 0.42" OLED
- [ ] **Photograph breadboard** — before disassembly
- [ ] **Draw schematic** — KiCad, Fritzing, or paper
- [ ] **Plan perfboard layout** — match schematic
- [ ] **Solder in sections** — power → MCU → analog → digital → connectors
- [ ] **Test after each section** — don't trust a fully-soldered board
- [ ] **Keep breadboard intact** — until perfboard fully works
- [ ] **Commit schematic to repo**

### Long-term hardware (not critical)

- [ ] **Consider high-side P-FET cutoff** on whole bike-mate rail (forget-proofing)
- [ ] **Consider TPL5110 low-power timer** (sleep draw <1µA)
- [ ] **Consider external antenna** for XCW SuperMini if WiFi range matters

---

## WHAT THIS BENCH CAN TEST

- Deep sleep / wake cycles
- BLE + GUI + time sync
- NTC temperature reading
- OLED rendering
- Ride pull from NVS
- Firmware boot / version / splash
- Wake log CSV growth

## WHAT THIS BENCH CANNOT TEST (yet)

- Real 12V voltage reading (PSU must be on)
- Buck-powered operation (USB currently)
- Real GPS fix (NEO-6M not wired)
- MOSFET ACC switch behaviour (not built)
- Sleep current measurement (needs DMM in series)
- Upload pipeline with real files (needs PSU on + files)
- Perfboard behaviour (not built)

---

## PARTS SOURCING

### From 830 tie-point breadboard kit
- Breadboard, jumper wires, Dupont wires
- Resistors: 100k, 10k, 1k, 330R, 220R, 1M, 2k, 5k1
- 104 ceramic caps (100nF)
- 100µF / 10µF electrolytic caps
- 1N4007 diodes
- PN2222 NPN transistors
- Green / red LEDs
- Active + passive buzzers

### From separate AliExpress orders
- XCW ESP32-C3 SuperMini
- 2× ESP32-C3 + 0.42" OLED
- 0.96" SSD1306 128x64 OLED
- 5× MP1584EN buck
- 2× QC3.0 USB buck
- 20× SA16CA TVS diodes
- 10× IRF4905 P-MOSFET
- 20× PN2222A NPN
- 10× MF52AT NTC 10k B3950
- 2× INA226 current monitor
- 10× A3144 Hall sensors
- 1× GY-NEO6M V2 NEO-6M GPS
- 2× 12V piezo buzzer
- 4× 12V rocker switch
- 40mm 5V PWM fan

---

*End of bench snapshot.*
