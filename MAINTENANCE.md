# Bike-Mate — Maintenance Mode

Architecture, entry/exit flow, safety cap, and what runs during maintenance.

The bike is asleep most of the time. To do anything interactive — watch serial,
flash firmware — the bike must be woken into **maintenance mode**.

---

## Summary

Maintenance mode means:

- The bike stays awake
- WiFi is up on the home network
- An HTTP server runs on port 80
- BLE is off (shared 2.4 GHz radio)
- The `/serial` page is live
- OTA is available via HTTP POST

It is user-triggered only, time-bounded, and returns to normal sleep when
it ends.

---

## Enter

**Preconditions:** bike is running normally (MONITOR, BLE advertising, WiFi off).

1. GUI → menu → **Activate Maintenance Mode**.
2. GUI writes `{"maint":"on"}` to the bike over BLE (OTA characteristic).
3. Firmware's BLE callback:
   - Sets `maintRequest = true` (RTC_DATA_ATTR, survives deep sleep)
   - Notifies `0x01` back to the GUI
   - Returns. Does **not** touch WiFi. The bike continues its normal loop.
4. Bike sleeps as usual when `shouldSleep()` returns true.
5. On the next wake, if `maintRequest` is already set at `setup()` time,
   `bleInit()` is skipped entirely — the radio goes straight to WiFi. This
   is the fix for intermittent association failures on the C3 in core
   2.0.17 after a BLE session.
6. The maint block in `loop()` sees `maintRequest`:
   - Prints `[MAINT] ====== ENTERING MAINTENANCE MODE ======`
   - Calls `wifiBringUp()` — two attempts with a 3 s settle between them
   - Calls `serverSetup()`
   - Records `maintStartMs`
   - Enters the maint loop
6. GUI is polling `http://<BIKE_IP>/serial-raw` every 5 s. When the bike's
   HTTP server comes up, the poll returns 200 and the GUI flips state to ON.
7. GUI opens the serial page in the browser (Yes/No dialog first).

The delay from click to serial page live is one bike wake cycle — up to
30 s bench, up to 5 min field (day), 10 min (night).

---

## During maintenance

Bike is awake. `doStateWork` is not called (the maint loop is a blocking
`while` inside the main `loop()`). What runs:

- `serverLoop()` — HTTP request handling
- `delay(20)` between iterations
- OLED redraw once per second — `drawMaintScreen()` shows `MAINT / MODE /
  <wifiMessage> / Ns left`. During HTTP OTA, `drawFwUpdateScreen()` takes
  over and shows `Ver X.YZ / Downloading… Flashing… Rebooting…`
- Voltage sample every 500 ms — if V crosses `runningEnter_mv`, the loop
  bails immediately and WiFi drops. Protects the prime directive against
  an unattended maint session racing a ride start.
- Checks for `/maint/off` signal (`maintOffRequested`)
- Checks `MAINT_MAX_MS` elapsed

BLE and the ride state machine are paused. Wake logging is paused via
`wakeLoggerPause()`.

### Serial page

`GET /serial` returns an HTML page. JS polls `GET /serial-raw` every 2 s.
`/serial-raw` returns the contents of the ring buffer in `SerialBuffer.cpp`.

The ring buffer holds the last 50 lines of `tprint()` output, each line
prefixed with `[HH:MM:SS.mmm]` and terminated with a newline.

Top of the page has an **End Maintenance** button that POSTs to `/maint/off`.

### OTA

`POST /ota?ver=X.YZ` with a multipart body containing
`firmware=@bike_mate.ino.bin`. Firmware streams the upload into `Update.h`,
runs a 5..1 countdown on the OLED, and reboots.

`GET /ota-progress` returns stage/bytes/total/countdown JSON. The GUI
polls it every 500 ms during the POST and prints stage transitions. Note:
Arduino `WebServer` strips `Content-Length` on multipart upload, so
`total` is always 0 — the GUI reports stage transitions and total elapsed
time, not a percent bar.

No auth. LAN only. See "Safety" below.

### Version

`GET /version` returns `BIKE_MATE_VERSION` as plain text. GUI uses this in
the OTA dialog when BLE is unavailable (which it is during maint).

---

## Exit

Four ways maintenance mode ends:

### 1. User clicks End Maintenance on the serial page

Browser POSTs `/maint/off`. The server sets `maintOffRequested = true`.
Next iteration of the maint loop sees it, breaks out, and:

- Prints `[MAINT] exiting (user)`
- Calls `serverStop()` → `[HTTP] server down`
- Calls `wifiBringDown()` → `[WIFI] down`, then `bleStart()`
- Clears `maintRequest = false`
- Resumes normal loop

### 2. GUI Deactivate menu

GUI POSTs `/maint/off` directly (does not need the browser). Same code path
as above. GUI then polls `/serial-raw` until it fails, at which point state
flips to OFF in the GUI.

### 3. Engine start detected

The maint loop samples voltage every 500 ms. If V crosses `runningEnter_mv`
(default 13.8 V), it prints `[MAINT] engine start detected (X.XXV), exiting`,
stops the HTTP server, drops WiFi, and hands control back to the normal
state machine. Protects the prime directive: an unattended maint session
must not hold WiFi up for 15 min while the engine runs.

### 4. Safety timeout

If nothing else ends it, the maint loop checks:

```c
if ((millis() - maintStartMs) > MAINT_MAX_MS) {
    tprint("[MAINT] exiting (timeout)");
    break;
}
MAINT_MAX_MS is 15 minutes (900000 ms) in Config.h.

This is the safety switch. If the user forgets, or the GUI crashes, the
bike still returns to sleep after 15 minutes.
What can go wrong
BLE ACK race

When the bike receives {"maint":"on"} over BLE, it sends the ACK notification
and returns. The main loop then processes the flag and calls wifiBringUp(),
which calls bleStop(). If the ACK hasn't finished transmitting when bleStop()
runs, the Mac never sees it.

Mitigation: the GUI does not rely on the ACK. It polls /serial-raw after
a fixed delay. The ACK is informational only.
WiFi bring-up failure

wifiBringUp() returns false if any step fails: associate, ping, or NTP sync.
If it fails, the maint loop is not entered, maintRequest is cleared, and the
bike returns to normal sleep. The GUI's poll times out after 11 minutes and
flips state to OFF with an error.

NTP is a hard gate. If pool.ntp.org is unreachable, maint fails.
GUI crash during maint

Bike is unaffected. It stays in maint until either the timeout fires or the
user reopens the GUI and clicks Deactivate.
GUI restart while bike is in maint

On startup, GUI probes /serial-raw once. If it responds, the GUI sets its
own state to ON. So a GUI restart mid-maint recovers cleanly.
Firmware implementation notes

Key files:

    Config.h — MAINT_MAX_MS

    BleManager.cpp — OtaCallbacks::onWrite parses {"maint":"on"}, sets
    maintRequest = true, sends ACK

    bike_mate.ino — maintRequest is RTC_DATA_ATTR bool. doStateWork
    consumes it. The maint loop lives in loop(), replacing the normal loop
    body while active.

    WebServer.cpp — HTTP server, all routes, maintOffRequested flag

    SerialBuffer.cpp — ring buffer feeding /serial-raw

State variables:

    maintRequest — RTC, set by BLE, consumed by loop

    maintStartMs — non-static global in bike_mate.ino, read by the OLED for the countdown

    maintOffRequested — volatile, set by /maint/off handler

    serverIsRunning() — returns the WebServer's running flag

All maint-state decisions are local. Nothing external polls or forces state.
GUI implementation notes

Key file: bikemate.py.

    maintenance_state — module-level string, "OFF" | "PENDING" | "ON"

    maintenance_lock — RLock, guards state reads and writes

    activate_maintenance() — flips PENDING, sends BLE command, starts poll

    deactivate_maintenance() — flips PENDING, POSTs /maint/off, starts poll

    _start_maint_poll() — 5 s interval, hits /serial-raw, flips ON on 200

    _start_maint_off_poll() — 2 s interval, waits for connection refused,
    flips OFF

    _check_maint_on_startup() — one-shot probe on launch, recovers state if
    bike is already in maint

    _ask_open_serial_page() — Yes/No dialog after ON

On OTA success, GUI sets state to OFF because cold boot wipes maintRequest.
What maint mode is NOT

    Not always-on. Does not run in the field.

    Not a replacement for the sleep state machine. The bike still sleeps on
    schedule when maint is off.

    Not a long-term debug session. 15 minutes max.

    Not authenticated. Anyone on the LAN who knows 192.168.8.196 can flash
    the bike. Acceptable for a home network; add a token before this leaves
    the LAN.

    Not a second BLE path. BLE and WiFi share the radio. Only one at a time.

Safety summary
Thing	Value	Where
Maint max duration	900000 ms (15 min)	Config.h
WiFi bring-up timeout	30 s (associate) + ping + NTP	WifiManager.cpp
Wake interval that pulls the bike into maint	30 s bench, 300/600 s field	Config.h
GUI poll timeout for entering maint	11 min	bikemate.py
GUI poll timeout for exiting maint	90 s	bikemate.py

If all else fails, the bike wakes, sees no flag, and sleeps. Worst case is
one wake cycle of extra draw.

If this file and the code disagree, the code wins. Fix this file.
