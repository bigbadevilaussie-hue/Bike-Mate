# === Bike-Mate GUI: ble (V5.34 / GUI 4.40) ===
# The GUI no longer runs a BLE client. OSA owns the BLE link to the
# bike. This module polls OSA's /bike/status and populates
# state.latest_data with the same schema the rest of the GUI reads.
#
# Class name kept as BLEWorker so app.py's `self.worker = BLEWorker()`
# still resolves. `client` and `loop` are None stubs for straggling
# references in app.py that haven't been ported yet.

import threading, time, json, urllib.request
from .config import OSA_URL, BLE_STALE_SEC
from . import state
from .helpers import set_status


class BLEWorker(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.running = True
        self.client = None       # stub: BLE client no longer exists
        self.loop = None         # stub: no asyncio loop
        self.last_ok = 0.0
        self.last_err = ""

    def run(self):
        while self.running:
            try:
                url = OSA_URL + "/bike/status"
                req = urllib.request.Request(url)
                with urllib.request.urlopen(req, timeout=3) as r:
                    body = r.read().decode()
                js = json.loads(body)
                data = js.get("data")
                if js.get("alive") and data:
                    # same shape the BLE notify used to deliver
                    state.latest_data.update(data)
                    state.latest_seen_time = time.time()
                    if "fv" in data:
                        state.device_version = data["fv"]
                    v = data.get("v")
                    t = data.get("t")
                    if v is not None and t is not None:
                        state.volt_hist.append(float(v))
                        state.temp_hist.append(float(t))
                    self.last_ok = time.time()
            except Exception as e:
                self.last_err = str(e)
            time.sleep(1.0)

    def send_ota_command(self, payload_json, result_callback):
        # Legacy hook still referenced by app.py::activate_maintenance.
        # Maint-on is now a POST to OSA. If this fires, it means the
        # app.py path hasn't been updated yet — report failure cleanly.
        result_callback(False, "BLE path retired; use OSA /bike/maint_on")


def send_maint_on(result_callback=None):
    """POST /bike/maint_on to OSA. Returns (ok, detail)."""
    try:
        req = urllib.request.Request(
            OSA_URL + "/bike/maint_on", method="POST")
        with urllib.request.urlopen(req, timeout=5) as r:
            body = r.read().decode()
        if result_callback:
            result_callback(True, body)
        return True, body
    except Exception as e:
        if result_callback:
            result_callback(False, str(e))
        return False, str(e)
