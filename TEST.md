# Bike-Mate Feature & Function Test List

**Vehicle:** 2008 Triumph Sprint ST 1050 (Australia)
**Battery:** VTX12-BS AGM (12V, 10.5 Ah)
**Prime Directive:** Never drain the battery below the cranking floor (~12.2V rested).

### Hardware & Power
1. **MOSFET Rail Switching:** Turns the switched 12V rail ON when engine is running (accState = true), powers GPS and USB.
2. **Sleep Current:** Drops to ~1 mA in deep sleep (MOSFET off, ESP32 sleeping).
3. **Voltage Divider:** Reads battery voltage accurately via GPIO 0.
4. **NTC Thermistor:** Reads ambient temperature accurately via GPIO 3.
5. **USB Charger:** 12V->5V USB module works when ACC is on.
6. **OLED Display:** Shows splash screen on boot.
7. **Green LED:** Indicates ACC state.
8. **Red LED:** Indicates PANIC / Alarm state.

### Core State Machine
9. **Timer Wake:** Wakes every 5 minutes (field mode) or 30 seconds (bench mode).
10. **Engine Start Detection:** Voltage > 13.8V triggers the state machine (chirps, starts ride log).
11. **Engine Stop Detection:** Voltage < 13.0V triggers ARMING countdown, then closes ride log.
12. **PANIC Mode:** Voltage < 12.2V triggers emergency state, sounds alarm, sends email.
13. **Reader for Rider:** Wake button starts a 45-second window for the rider to check the bike.
14. **Secret Menu:** Double-click wake button shows V/T/Stor/Time/Date for 30 seconds (Red LED on).
15. **Cool Down Mode:** After a ride, holds the board awake for 30 seconds with silent 1Hz wake logging.

### Logging & Data
16. **Wake Logger:** Writes a CSV row every wake (timestamp, voltage, temp, state, flags, sats).
17. **Ride Logger:** Writes a CSV row every 5 seconds while the engine is running (includes GPS speed, altitude, course).
18. **GPS Parsing:** Reads NMEA sentences from the NEO-6M on GPIO 2.
19. **Clock Sync:** Syncs time from Opal LAN router (NTP fallback).
20. **Gzip & Seal:** Compresses wake/ride files and deletes the raw data to save space.
21. **BLE Upload:** Sends sealed files over BLE (26b1) to OSA when connected.
22. **WiFi Upload:** Sends sealed files over HTTP to OSA during maintenance mode.

### Connectivity & OTA
23. **BLE Telemetry:** Broadcasts JSON data (V/T/State) over 26a8 to OSA.
24. **BLE Heartbeat:** Broadcasts heap/uptime over 26ab to OSA.
25. **Maintenance Mode:** BLE turns off, WiFi turns on. Bounded to 5 minutes.
26. **HTTP Server:** Serves `/serial`, `/serial-raw`, `/version`, `/settings`, `/upload`, `/seal`, `/ota`.
27. **OTA Update:** Allows flashing a new `.bin` over WiFi via HTTP POST.

### GUI & OSA
28. **OSA Hub:** Bridges BLE telemetry and files to the GUI and GitHub.
29. **GUI Monitoring:** Polls OSA for live telemetry, displays graphs, and shows current state.
30. **GUI Reports:** Reads wake/ride logs from OSA and draws voltage/temp graphs.
31. **Ride Finder:** Lists all rides from OSA with human-readable dates.
32. **DynaTune/DynaRide:** (Placeholder) Displays diagnostic reports.

### Pending/Failing Tests
- [ ] GPS UART data reception (currently `avail=0` on GPIO 2).
- [ ] Active buzzer volume (pending V6.07 patch verification).
- [ ] 7-day field test on the Sprint ST 1050.
