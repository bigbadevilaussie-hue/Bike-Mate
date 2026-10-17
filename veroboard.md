# Bike-Mate — Veroboard Build Guide

Firmware V5.64 · Board revision V6 · 2026-10-15

---

## 1. Board

| Spec | Value |
|---|---|
| Length | 95 mm |
| Width | 65 mm |
| Thickness | ~1.6 mm |
| Copper strips | 37, running lengthwise (left to right) |
| Holes per strip | 24 |
| Hole pitch | 2.54 mm (0.1 in) |
| Strip pitch | 2.54 mm |
| Cutting required | Yes — from larger stock board |

Count 24 holes on the actual board before cutting.

---

## 2. Stripboard layout — flow order (left to right)

TVS -> MP1584EN -> ESP32-C3 -> control section -> IRF4905 -> Mini360 -> GPS -> USB charger
text


Three logical blocks:

1. Power in — TVS, fuse, master switch, MP1584EN
2. Controller — ESP32-C3 SuperMini + control section
3. Switched rail — IRF4905, Mini360, GPS, USB charger

---

## 3. Zone map

| Zone | Position | Contents |
|---|---|---|
| A Power in | Left end | TVS (D1), fuse (F1), master switch, MP1584EN (M1) |
| B Controller | Left-centre | ESP32-C3 (U1), divider R1/R2 + C1 |
| C Control | Centre | LED resistors (R5, R3), I2C pull-ups (R11, R12), IRF driver (R7, R8, Q1), buzzer driver (R9, R10, Q2), NTC divider (R4 + RT1 header) |
| D Switched | Right-centre | IRF4905 (U2), Mini360 (M2) |
| E Output | Right end | GPS (M4), USB charger (M3) |

Exact columns per zone determined on the bench after module placement.

---

## 4. Cut list

Cut positions determined on the bench after module placement. At minimum:

| Location | Purpose |
|---|---|
| Under ESP32-C3 headers | Isolate GPIO rows |
| Between adjacent zones | Isolate 12V, 5V, GND rails |
| Under each module | Cut any strip that would short module pins |

Continuity-check every cut with a multimeter after cutting.

---

## 5. BOM

| Ref | Qty | Part | Value / Spec | Location |
|---|---|---|---|---|
| U1 | 1 | ESP32-C3 SuperMini | XCW, USB-C, 4 MB flash | Zone B |
| M1 | 1 | Buck module | MP1584EN, 12V to 5V, always-on | Zone A |
| M2 | 1 | Buck module | Mini360, 12V to 5V, switched | Zone D |
| M3 | 1 | USB charger | Single USB-A, 12V input | Zone E |
| M4 | 1 | GPS module | NEO-6M, 3.3-5V input | Zone E |
| U2 | 1 | P-MOSFET | IRF4905, TO-220 | Zone D |
| Q1 | 1 | NPN transistor | 2N2222 or PN2222, TO-92 | Zone C |
| Q2 | 1 | NPN transistor | 2N2222 or PN2222, TO-92 | Zone C |
| D1 | 1 | TVS diode | SA16CA | Zone A |
| D2 | 1 | LED red | 5 mm, panel mount | Front panel (fly via J6) |
| D4 | 1 | LED green | 5 mm, panel mount, "ACC on" | Front panel (fly via J7) |
| RT1 | 1 | Thermistor | MF52AT 10k NTC, B=3950 | Front panel (fly via J3) |
| R1 | 1 | Resistor | 100 kohm | Zone B |
| R2 | 1 | Resistor | 10 kohm | Zone B |
| R3 | 1 | Resistor | 10 kohm | Zone C |
| R4 | 1 | Resistor | 10 kohm | Zone C |
| R5 | 1 | Resistor | 330 ohm | Zone C, red LED |
| R6 | 1 | Resistor | 10 kohm | Zone D gate pull-up |
| R7 | 1 | Resistor | 1 kohm | Zone C Q1 base |
| R8 | 1 | Resistor | 10 kohm | Zone C Q1 base pull-down |
| R9 | 1 | Resistor | 1 kohm | Zone C Q2 base |
| R10 | 1 | Resistor | 10 kohm | Zone C Q2 base pull-down |
| R13 | 1 | Resistor | 330 ohm | Zone C, green LED (on GPIO1) |
| C1 | 1 | Capacitor | 100 nF ceramic | Zone B ADC filter |
| C2 | 1 | Capacitor | 100 nF ceramic | Zone B SuperMini decoupling |
| BZ1 | 1 | Buzzer | SFM-27, 3-24V, 12V nominal | Fly via J5 |
| SW1 | 1 | Rocker switch | YJ-SW019, SPST, 20A, 20 mm hole | Right side panel (off-board, via J4) |
| J1 | 1 | Connector | 2-pin, 12V battery input | Zone A |
| J2 | 1 | Connector | 4-pin, OLED I2C | Fly to OLED |
| J3 | 1 | Connector | 2-pin, NTC | Fly to panel |
| J4 | 1 | Connector | 2-pin, 12V master switch | Fly to switch |
| J5 | 1 | Connector | 2-pin, buzzer | Fly to buzzer |
| J6 | 1 | Connector | 2-pin, red LED | Fly to panel |
| J7 | 1 | Connector | 2-pin, green LED | Fly to panel |
| J8 | 1 | Connector | 2-pin, wake button | Fly to panel |
| OLED | 1 | Display | SSD1306, 0.96 in, I2C, addr 0x3C | Front panel |
| — | 1 | Stripboard | 95 x 65 mm, 37 strips x 24 holes | — |
| — | 1 | Enclosure | Remix of kaspergrontved ESP32 WROOM case | — |
| — | — | Wire, solder, headers | 24 AWG solid, 0.6 mm solder, 2.54 mm female headers | — |

### Dropped from the original design

These parts appear in the KiCad schematic but are **not on the final vero board**:

| Ref | Part | Value | Why |
|---|---|---|---|
| D3 | Zener diode | 1N4742A 12V 1W | Not fitted (gate clamp not needed) |
| R11 | Resistor | 4.7 kohm | OLED has its own I2C pull-ups |
| R12 | Resistor | 4.7 kohm | OLED has its own I2C pull-ups |
| C3 | Capacitor | 22 uF 16V | Not fitted |
| C4 | Capacitor | 100 nF | Not fitted |
| F1 | Fuse | 3 A | External on the 12V wire, not on the board |

---

## 6. Flying leads (panel items)

| Signal | GPIO | From board | To panel |
|---|---|---|---|
| OLED VCC | — | +3V3 | OLED VCC |
| OLED GND | — | GND | OLED GND |
| OLED SDA | GPIO 6 | U1 pin GPIO6 | OLED SDA |
| OLED SCL | GPIO 9 | U1 pin GPIO9 | OLED SCL |
| Red LED anode | GPIO 10 | R5 output | D2 anode |
| Red LED cathode | — | GND | D2 cathode |
| Green LED anode | — | R13 output (GPIO1) | D4 anode |
| Green LED cathode | — | +12V | D4 cathode |
| NTC lead 1 | GPIO 3 | top of R4 | RT1 pin 1 |
| NTC lead 2 | — | GND | RT1 pin 2 |
| Wake button 1 | GPIO 5 | U1 pin GPIO5 | J8 pin 1 |
| Wake button 2 | — | GND | J8 pin 2 |
| Buzzer + | — | +12V_RAIL | BZ1 red |
| Buzzer - | — | Q2 collector | BZ1 black |

GPS module is on-board (Zone E), not a flying lead.

---

## 7. Enclosure

Base design: remix of kaspergrontved ESP32 WROOM + SSD1306 + 18650 case.

| Face | Feature | Size |
|---|---|---|
| Front | OLED window | Existing cutout (inherit from base) |
| Front | Red LED | 5 mm hole |
| Front | NTC | 3 mm hole |
| Front | Green LED | 5 mm hole |
| Front | USB-A cutout | ~15 x 15 mm |
| Right side | Wake button | Tactile switch hole |
| Right side | On/off rocker | 20 mm hole |
| Right end | 12V wire grommet | Grommet size TBD |
| Top | GPS antenna slot | ~20 x 20 mm above GPS module |
| Inside | Buzzer vent holes | TBD on bench |
| — | 18650 battery bay | Left as-is (unused) |

ESP32 USB-C hidden inside case — firmware via OTA only.

---

## 8. Power chain

12V in (grommet, right end)
|
v
J1 --> J4-1 --> [SW1 off-board] --> J4-2 --> +12V_RAIL
|
+-------------------------------+---------------------------+
| | |
D1 TVS M1 MP1584EN U2 IRF4905
(12V rail clamp) 12V to 5V always-on P-MOSFET source
| | |
GND +5V_ALWAYS U2 drain -> SWITCHED_12V
| |
| +------------+------------+
| | |
| M2 Mini360 M3 USB charger
| 12V to 5V switched (12V in, USB-A)
| |
| +5V_SWITCHED
| |
| +-------+-------+
| | |
| M4 GPS
| (3.3-5V in) (optional)
|
U1 ESP32-C3
+5V pin (always-on)
+3V3 pin -> OLED, NTC, I2C pull-ups
---

## 9. Build order

1. Trim board to 100 x 35 mm with 18 intact strips and 40 holes per strip.
2. Place modules dry (no solder) in flow order.
3. Mark strip positions for each module pin. Identify cuts to avoid shorts.
4. Make all cuts. Continuity-check every cut.
5. Solder smaller parts first: resistors, ceramic caps, transistors, TVS, P-MOSFET.
6. Solder modules.
7. Solder C3 (tallest part) last.
8. Add flying leads for panel items.
9. Continuity check: no short between +12V_RAIL and GND, no unintended connections.
10. Power up without ESP32-C3. Confirm 5V on +5V_ALWAYS, 5V on +5V_SWITCHED when switched rail is on.
11. Insert ESP32-C3. Confirm boot, OLED splash, BLE advertising.
12. Measure sleep current.
13. Seal enclosure.

---

## 10. Power budget

### Always-on (ACC off)

| Load | Current |
|---|---|
| MP1584EN idle | 1-3 mA (claims 0.1 mA, measure) |
| R1/R2 divider | ~116 uA |
| ESP32-C3 light sleep | 30 uA - 10 mA (variant dependent) |
| Total | MEASURE ON REAL BOARD |

### Switched rail (ACC on)

| Load | Current |
|---|---|
| Mini360 + GPS | ~200 mA |
| USB charger (charging phone) | up to 2 A |
| Fan (if fitted) | ~100 mA |
| Total | up to ~2.3 A |

3 A fuse is correct.

---

## 11. Known issues

- Prime directive risk: MP1584EN idle (1-3 mA) plus SuperMini LDO (up to 5 mA) may total 5-8 mA on the always-on rail. On a 10.5 Ah AGM that is ~20-30% per month. Measure sleep current before installing on the bike. If over 2 mA, consider swapping MP1584EN for a TPS54302 low-Iq buck.
- GPIO 9 boot strap: held high at boot by R12 (4.7k to 3V3). If OLED is not connected, add temporary 10k pull-up to 3V3.
- GPIO 0 boot strap: held at ~1.1 V by R1/R2 when 12V present. Below ~2V may enter download mode. Ensure 12V present before powering ESP32 on the bench.
- GPS antenna slot: above ceramic patch, not shadowed by case plastic.
- Buzzer vent: SFM-27 is loud (~95 dB) but muffled by sealed plastic. Add 3-5 small holes near buzzer.
- No reverse polarity protection beyond polarised connector.
- ESP32 USB-C hidden: no recovery via USB once case is closed. OTA only.

---

## 12. Version history

| Version | Date | Change |
|---|---|---|
| V5.63 | 2026-10-15 09:50 | Old layout, 20 strips x 35 holes. Superseded. |
| V6 | 2026-10-15 | New layout, 18 strips x 40 holes, flow order. Superseded. |
| V7 | 2026-10-17 | Board 37 strips x 24 holes, 95 x 65 mm. Module footprints added. J4 = 12V master switch header. M3 = single USB-A. Green LED = MOSFET-on indicator. Buzzer off-board via J5. |

---

## 13. Assembly photos

- bike_mate_veroboard_layout.png — visual strip map
- bike_mate_veroboard_layout.svg — vector version
- bike_mate_veroboard.xlsx — build tracker

---

## 14. ESP32-C3 antenna mod

The XCW SuperMini's onboard PCB antenna is poorly matched. Its
documented failure mode is WiFi association failing after a BLE
session (see README V4.91 and WifiManager.cpp).

**Fix applied to this build:**
- Onboard antenna disconnected (trace cut / ceramic chip removed)
- 31 mm straight wire soldered to the ANT feed point
- 31 mm = quarter-wave at 2.4 GHz (125 mm / 4 = 31.25 mm)

**Wire routing:**
- Straight, not coiled
- Clear of stripboard and case walls by >=5 mm
- Vertical in use if possible
- Case may need a notch or internal standoff for clearance

**Expected effect:**
- WiFi range: 10-30 m improvement typical
- BLE stability: fewer disconnects, more reliable uploads
- May reduce or eliminate the WiFi-after-BLE association
  failures the V5.55 retry logic was added for. Leave the
  retries in place regardless.

---

## 15. Module footprints

Physical dimensions for the five on-board modules, in stripboard holes (2.54 mm pitch).

| Ref | Module | Body W x H (holes) | Pin count | Pin layout |
|---|---|---|---|---|
| M1 | MP1584EN (black board) | 6 x 8 | 4 | 4 corners |
| M2 | Mini360 | 5 x 7 | 4 | 4 corners |
| M3 | USB charger (single USB-A) | 5 x 15 | 2 | top edge (IN+, GND) |
| M4 | NEO-6M GPS | 9 x 13 | 5 | bottom edge (VCC, GND, TXD, RXD, PPS) |
| U1 | ESP32-C3 SuperMini | 6 x 8 | 16 (8 per side) | left col top-to-bottom: 5V, GND, 3V3, GPIO4, GPIO3, GPIO2, GPIO1, GPIO0; right col top-to-bottom: GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, GPIO20, GPIO21 |

All other on-board parts use stock stripboard footprints and need no special sizing.

All J-connectors are "pretend" — the wires solder directly to the board. The header blocks exist in the netlist to define connectivity, not as physical parts.
