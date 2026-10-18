# Bike-Mate — Veroboard Build Guide

Hardware V8 (final) · Firmware V5.66 · 2026-10-17

Final circuit unless the field test fails. The schematic in the stripboard
editor is the source of truth. Names below match the schematic labels.

---

## 1. Board

| Spec | Value |
|---|---|
| Tool | stripboard-editor.com |
| Rows | 24 |
| Columns | 37 |
| Pitch | 2.54 mm |

Count the real board before cutting. Rule: two legs of one part never share an
uncut strip. Continuity-check every cut.

---

## 2. Schematic (nets)

| Net | Connects |
|---|---|
| BATT+ | AGM-BATT 1, 12vSwitch 2 |
| +12V_RAIL (behind switch) | 12vSwitch 1, TVS A, M1 12V, MOSFET S, R4 bottom, R1 top, BUZZER 1 |
| +12V_SW (+12v-Load) | MOSFET D, Mini360 IN+, USB IN+ |
| +5V (always) | M1 5V, ESP32 5V |
| +5V_SW (+5v-Load) | Mini360 OUT+, GPS VCC |
| 3V3 | ESP32 3V3, R3 top, C2, OLED 3V3 |
| GND | AGM-BATT 2, TVS K, M1 GND, Mini360 IN- and OUT-, USB GND, GPS GND, Q1 E, Q2 E, R2 bottom, C1, C2, R8, R10, NTC 2, Green LED 2, Red LED 2, Wake 2, OLED GND, ESP32 GND |
| GATE | MOSFET G, R4 top, Q1 C |
| Q1 base | R5 (to GPIO7), R8 (to GND) |
| Q2 base | R9 (to GPIO4), R10 (to GND) |
| BUZZER 2 | Q2 C |
| VBAT_SENSE | R1 bottom, R2 top, C1, GPIO0 |
| NTC node | R3 bottom, NTC 1, GPIO3 |
| GPS TXD | R6 to GPIO2 |
| Green LED | GPIO1, R7, Green LED 1 |
| Red LED | GPIO10, R11, Red LED 1 |
| Wake | GPIO5, Wake 1 |
| OLED | SDA = GPIO6, SCL = GPIO9 |

GPS RXD and PPS are not connected.

### GPIO map

| GPIO | Use | Config.h |
|---|---|---|
| 0 | Battery voltage | VOLTAGE_PIN |
| 1 | Green LED (follows accState) | ACC_LED_PIN |
| 2 | GPS RX | GPS_RX_PIN |
| 3 | NTC | THERMISTOR_PIN |
| 4 | Buzzer (Q2) | BUZZER_PIN |
| 5 | Wake button | WAKE_BTN_PIN |
| 6 | OLED SDA | SDA_PIN |
| 7 | MOSFET driver (Q1), same signal as ACC_LED_PIN | MOSFET_PIN |
| 9 | OLED SCL | SCL_PIN |
| 10 | Red LED | STATUS_LED_PIN |
| 8, 20, 21 | Free | |

---

## 3. BOM

### On the board

| Ref | Part | Value / spec | Qty |
|---|---|---|---|
| ESP32 | ESP32-C3 SuperMini (XCW) | USB-C, 4 MB | 1 |
| M1 | MP1584EN buck module | 12 V to 5 V | 1 |
| Mini360 | Buck module | 12 V to 5 V, switched | 1 |
| USB | USB-A charger module | 12 V in, 38 x 19 mm, wired by its two pads | 1 |
| GPS | NEO-6M | VCC, GND, TXD used | 1 |
| MOSFET | IRF4905 | P-channel, TO-220 (G, D, S left to right) | 1 |
| Q1, Q2 | PN2222 / 2N2222 | NPN, TO-92. Check pinout first | 2 |
| TVS | SA16CA | Bidirectional | 1 |
| R1 | Resistor | 100 K | 1 |
| R2, R3, R4 | Resistor | 10 K | 3 |
| R5, R6, R9 | Resistor | 1 K | 3 |
| R7, R11 | Resistor | 330 ohm | 2 |
| R8, R10 | Resistor | 5K1 | 2 |
| C1, C2 | Ceramic capacitor | 100 nF | 2 |
| — | Stripboard | 24 rows x 37 columns | 1 |
| — | Wire, solder, 2.54 mm headers | | — |

### Off board (wired to pads or headers)

| Name | Part | Notes |
|---|---|---|
| AGM-BATT | 12 V battery lead | 3 A inline fuse at the battery, SAE connector |
| 12vSwitch | Rocker switch, SPST 20 A | 20 mm hole |
| BUZZER | SFM-27, 12 V | + on 12V_RAIL, - on Q2 C |
| NTC | 10 K, MF52AT, B=3950 | Measure it: about 10 K at room temperature |
| Green LED | 5 mm | Anode = pin 1 |
| Red LED | 5 mm | Anode = pin 1 |
| Wake | Tactile button | |
| OLED | SSD1306 0.96", I2C, 0x3C | Match header order to module printing |

---

## 4. Notes

- 12vSwitch kills everything, including the ESP32 and M1. No sleep or wake with it off.
- MOSFET driver is on GPIO7 (MOSFET_PIN). Green LED is on GPIO1 (ACC_LED_PIN). Both follow accState.
- Wake button has no external pull-up. Enable the internal one in firmware.
- R8 and R10 are pull-downs. They keep the MOSFET off and the buzzer quiet while the ESP32 sleeps or boots.
- No reverse-polarity protection. Mark + on the SAE lead and the board.
- Check SuperMini pin labels on the real board against the schematic symbol before placing.
- Measure sleep current before fitting to the bike.
