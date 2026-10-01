# Bike-Mate — Project State

Snapshot date: 2026-10-01
Firmware: see `Config.h` → `BIKE_MATE_VERSION`
GUI: see `bikemate.py` → `GUI_VERSION`
Repo: https://github.com/bigbadevilaussie-hue/Bike-Mate

---

## PROJECT PHILOSOPHY

Rule 1 — Have fun
Rule 2 — Over engineer it
Rule 3 — It's useless but look cool
Rule 4 — Fuck it lmao

**Prime directive overrides everything else:**
Bike-Mate must not drain the bike battery past the point where it will
not crank the engine. On the Sprint ST 1050 with a stock YT12B-BS AGM,
the practical cranking floor is ~12.2V rested.

---

## HARDWARE

### Current bench
- XCW ESP32-C3 SuperMini (no onboard OLED)
- External 0.96" SSD1306 128x64 OLED (I2C, addr 0x3C)
- MP1584EN buck module (12V → 5V)
- 12V bench PSU (30V / 3A)
- Breadboard + jumpers

### Analog front-ends
- Voltage divider 100k / 10k on GPIO 0 — **no filter cap yet**
- MF52AT NTC 10k (Beta 3950) on GPIO 3, with 100nF ceramic cap

### Digital I/O
| Function | GPIO |
|---|---|
| Voltage sense | 0 |
| ACC LED / MOSFET gate | 1 |
| Thermistor | 3 |
| Buzzer | 4 |
| OLED SDA | 6 |
| OLED SCL | 9 |
| Status LED | 10 |
| GPS RX / TX | TBD (not wired) |
| Switched 12V rail (PN2222A) | TBD (not wired) |

Note: GPIO9 is a boot-strapping pin on the ESP32-C3.

### Not yet wired
- GY-NEO6M V2 NEO-6M GPS (in hand)
- IRF4905 P-MOSFET + PN2222A for switched 12V rail (in hand, LED sim only)
- 12V rocker switch (in hand, bench only)
- 12V piezo buzzer (in hand)
- INA226 current monitor (in hand, not in critical path)
- 40mm 5V PWM fan (in hand)

### Power

### Working tree

### Tags

Run `git log --oneline -5`, `git status --short`, and `git tag -l` and
paste the output here if this is stale.

---

## CURRENT VERSIONS

- **Firmware:** see `Config.h` → `BIKE_MATE_VERSION`
- **GUI:** see `bikemate.py` → `GUI_VERSION`
- **Latest audit:** `AUDIT.md` (tagged `audit-2026-10-01`)
- **Latest TODO:** `TODO.md` (tagged `todo-2026-10-01`)

---

## CURRENT STATUS

- Firmware, GUI, BLE, OTA, ride logging, wake logging, Drive upload, email
  alerts — all functionally working.
- Deep sleep tested and stable.
- Hardware: breadboard. Perfboard pending.
- GPS not yet wired. MOSFET ACC switch not yet wired.
- Sleep current not yet measured.

---

## TOP PRIORITIES

See `TODO.md` for the full ranked list. Short version:

1. Set `BENCH_MODE 0` before any deployment
2. Fix `readSensors()` early-return (stale voltage on fault)
3. Raise panic threshold 12.0 → 12.2V
4. Move MD5 verification before `Update.end(true)`
5. Fix PANIC sleep behaviour

---

## KNOWN ISSUES

See `AUDIT.md` for the full list. Highlights:

- PANIC blocks sleep — death spiral in the worst state
- `readSensors()` leaves stale voltage on ADC fault
- OTA MD5 verified after commit
- Three voltage calibrations exist, wrong one live
- 302 treated as unconditional success — deletes local files on Drive-side failure
- `_mailFailCount` never decays
- Docs stale (this file, README, PROJECT_STATE, FILES)

---

## SAFETY REMINDERS

- `Config.local.h` contains WiFi + Gmail credentials. Gitignored.
  If it has ever been shared, rotate the credentials.
- Apps Script URL is unauthenticated. Fix before real GPS data lands.
- BENCH_MODE 1 is compiled in — do not deploy on the bike.

---

## FILES IN REPO

See `FILES.md` for the full index. Quick list:

Firmware: `bike_mate.ino`, `Config.h`, `Sensors.*`, `Buzzer.*`,
`RideLogger.*`, `RideStorage.*`, `WakeLogger.*`, `BleManager.*`,
`DisplayManager.*`, `WifiManager.*`, `WifiMail.*`, `DriveUpload.*`,
`OtaManager.*`

GUI: `bikemate.py`

Docs: `README.md`, `PROJECT_STATE.md`, `HANDOFF.md`, `FILES.md`,
`AUDIT.md`, `TODO.md`

Build: `update_handoff.sh`

Secrets: `Config.local.h` (never commit)

---

*If the docs and the code disagree, the code wins. Fix the docs.*
