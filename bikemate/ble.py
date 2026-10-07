# === Bike-Mate GUI: ble ===
# auto-extracted, edit here ===

import asyncio, json, struct, threading, time
from bleak import BleakScanner, BleakClient
from .config import *
from . import state
from .helpers import *


class BLEWorker(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.running = True
        self.client = None
        self.loop = None

    def run(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self._loop())

    async def _pull_latest_ride(self, client):
        push_done = asyncio.Event()
        current = {"summary": None, "rows": []}
        newest = {"summary": None, "rows": []}

        def flush():
            if current["summary"] and current["rows"]:
                if current["summary"].get("durationSecs", 0) > 0:
                    if (newest["summary"] is None or
                        current["summary"]["startEpoch"] > newest["summary"]["startEpoch"]):
                        newest["summary"] = current["summary"]
                        newest["rows"] = list(current["rows"])
            current["summary"] = None
            current["rows"] = []

        def on_stream(sender, data):
            data = bytes(data)
            if len(data) >= 48:
                s = parse_summary(data)
                if s and s["rowCount"] > 0:
                    flush()
                    current["summary"] = s
                    current["rows"] = []
                    return
            for i in range(0, len(data) - 7, 8):
                r = parse_row(data[i:i+8])
                if r:
                    current["rows"].append(r)

        def on_request(sender, data):
            d = bytes(data)
            if not d:
                return
            c = d[0]
            if c == 0x00 or c == 0x02:
                flush()
                push_done.set()

        try:
            await client.start_notify(STREAM_UUID, on_stream)
            await asyncio.sleep(0.3)
            await client.start_notify(REQUEST_UUID, on_request)
            await asyncio.sleep(1.5)
            await client.write_gatt_char(REQUEST_UUID, (0).to_bytes(4, "little"))
            t0 = time.time()
            while time.time() - t0 < PULL_TIMEOUT_SEC:
                if push_done.is_set():
                    break
                if not client.is_connected:
                    break
                await asyncio.sleep(0.1)
            if not push_done.is_set():
                flush()
            try:
                await client.stop_notify(STREAM_UUID)
                await client.stop_notify(REQUEST_UUID)
            except Exception:
                pass
            if newest["summary"]:
                with state.latest_ride_lock:
                    state.latest_ride = newest
                print(f"[RIDE] latest: {newest['summary']['startEpoch']} dur={newest['summary']['durationSecs']}s")
        except Exception as e:
            print(f"[RIDE] pull err: {e}")

    def send_ota_command(self, payload_json, result_callback):
        if not self.loop:
            result_callback(False, "worker not running")
            return

        def _wait_and_send():
            t0 = time.time()
            last_status = 0
            while time.time() - t0 < OTA_WAIT_SEC:
                if self.client and self.client.is_connected:
                    break
                if time.time() - last_status > 15:
                    last_status = time.time()
                    elapsed = int(time.time() - t0)
                    set_status(f"waiting for device to wake ({elapsed}s / {OTA_WAIT_SEC}s)")
                time.sleep(0.5)

            if not (self.client and self.client.is_connected):
                result_callback(False, "device did not wake within 15 min")
                return

            print("[OTA] device connected, sending payload")
            fut = asyncio.run_coroutine_threadsafe(
                self._send_ota_async(payload_json), self.loop)
            try:
                ok, msg = fut.result(timeout=OTA_TIMEOUT_SEC + 5)
            except Exception as e:
                ok, msg = False, f"timeout/wait: {e}"
            result_callback(ok, msg)

        threading.Thread(target=_wait_and_send, daemon=True).start()

    async def _send_ota_async(self, payload_json):
        if not self.client or not self.client.is_connected:
            return False, "not connected"
        ack_event = asyncio.Event()
        ack_result = {"ok": False}

        def on_ack(sender, data):
            b = bytes(data)
            if len(b) >= 1:
                ack_result["ok"] = (b[0] == 0x01)
                ack_event.set()

        try:
            await self.client.start_notify(OTA_UUID, on_ack)
            await asyncio.sleep(0.3)
            await self.client.write_gatt_char(OTA_UUID, payload_json.encode())
            print("[OTA] payload written, waiting for ACK...")
            try:
                await asyncio.wait_for(ack_event.wait(), timeout=OTA_TIMEOUT_SEC)
            except asyncio.TimeoutError:
                try:
                    await self.client.stop_notify(OTA_UUID)
                except Exception:
                    pass
                return False, "ACK timeout"
            try:
                await self.client.stop_notify(OTA_UUID)
            except Exception:
                pass
            if ack_result["ok"]:
                print("[OTA] ACK received")
                return True, "ACK"
            else:
                print("[OTA] NACK received")
                return False, "NACK"
        except Exception as e:
            print(f"[OTA] error: {e}")
            return False, str(e)

    async def _loop(self):

        def on_data(sender, data):
            # V4.34: reassemble BLE fragments. macOS bleak splits any
            # notify larger than the negotiated MTU into multiple
            # callbacks. The firmware sends ~130 bytes of JSON; that
            # crosses the ~100-byte effective MTU and arrives in 2
            # pieces. Buffer until the JSON parses, then dispatch.
            payload = bytes(data)
            if not payload:
                return
            if not hasattr(on_data, "buf"):
                on_data.buf = b""
            # V4.34: a new JSON object starting means the previous
            # cycle's partial never completed (fragment dropped).
            # Discard the stale buffer rather than concatenating two
            # half-payloads from different cycles.
            if payload.lstrip().startswith(b"{") and on_data.buf:
                on_data.buf = b""
            on_data.buf += payload
            try:
                d = json.loads(on_data.buf.decode())
            except json.JSONDecodeError:
                # Not complete yet. Cap the buffer so a genuinely bad
                # payload can't grow without bound.
                if len(on_data.buf) > 512:
                    if time.time() - state.last_parse_fail > 10:
                        state.last_parse_fail = time.time()
                        print(f"[NOTIFY] parse fail (buf reset) raw={on_data.buf[:100]}")
                    on_data.buf = b""
                return
            except Exception as e:
                on_data.buf = b""
                if time.time() - state.last_parse_fail > 10:
                    state.last_parse_fail = time.time()
                    print(f"[NOTIFY] parse fail: {e} raw={payload[:100]}")
                return
            on_data.buf = b""
            state.latest_data.update(d)
            state.latest_seen_time = time.time()
            if "fv" in d:
                if state.device_version != d["fv"]:
                    state.device_version = d["fv"]
                    print(f"[DEVICE] firmware version: {d['fv']}")
            if "v" in d and "t" in d:
                v = float(d["v"])
                t = float(d["t"])
                if t < 24.0:
                    print(f"[TSPIKE] raw={json.dumps(d)}")
                state.volt_hist.append(v)
                state.temp_hist.append(t)

        while self.running:
            try:
                set_status("")
                dev = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=8.0)
                if not dev:
                    await asyncio.sleep(1)
                    continue
                set_status("connecting...")
                print(f"[BLE] connecting {dev.address}")
                try:
                    async with BleakClient(dev, timeout=10.0) as c:
                        self.client = c
                        set_status("connected")
                        print("[BLE] connected")
                        try:
                            await c.write_gatt_char(TIME_UUID, int(time.time()).to_bytes(4, "little"))
                        except Exception as e:
                            print(f"[TIME] {e}")
                        try:
                            await c.start_notify(DATA_UUID, on_data)
                            print("[BLE] subscribed DATA")
                        except Exception as e:
                            print(f"[DATA-SUB] {e}")
                        try:
                            v = await c.read_gatt_char(DATA_UUID)
                            on_data(None, v)
                        except Exception as e:
                            print(f"[DATA-READ] {e}")

                        try:
                            await self._pull_latest_ride(c)
                        except Exception as e:
                            print(f"[RIDE] {e}")

                        idle0 = time.time()
                        while c.is_connected:
                            await asyncio.sleep(1.0)
                            st = state.latest_data.get("s", "MONITOR")
                            if st != "MONITOR" and time.time() - idle0 > 60:
                                print("[BLE] idle timeout")
                                break
                except Exception as e:
                    print(f"[BLE] conn err: {e}")
                    set_status(f"err: {e}")
                    await asyncio.sleep(2)
                finally:
                    self.client = None
            except Exception as e:
                set_status(f"err: {e}")
                print(f"[BLE] loop err: {e}")
                await asyncio.sleep(3)

