"""
BIKE-MATE GUI V3.11
Paired with firmware V3.64+.

Version log:
  V3.01       OTA uses mDNS hostname instead of LAN IP
  V3.02       OTA payload includes firmware version
  V3.10       Sync from Drive, backup firmware to Drive, second local server,
              SSL context fix, Apps Script download endpoint
  V3.11       10-min delay before firmware backup upload (upstream contention)
"""

import asyncio, json, struct, threading, time, os, urllib.request, urllib.parse, subprocess, socket, hashlib, re, shutil, ssl, base64
import tkinter as tk
from collections import deque
from datetime import datetime
from tkinter import messagebox
from bleak import BleakScanner, BleakClient

GUI_VERSION = "3.11"
DEVICE_NAME = "Bike-Mate"
DATA_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
TIME_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9"
STREAM_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ab"
REQUEST_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ad"
OTA_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ae"

DRIVE_FOLDER_URL = "https://drive.google.com/drive/folders/1I48SYu8vTC4CDTFLUhZh53siI8ULbQ8W"
UPLOAD_URL = "https://script.google.com/macros/s/AKfycbz3xknkHcub61nntHvPqYrTFEmhKqHn_S1GhoUya0kAzrf-N89lIP6JA9tmpzSJV6mL/exec"

OTA_DIR = os.path.expanduser("~/bike-mate-ota")
OTA_PORT = 8000
OTA_HOSTNAME = "Familys-iMac"
OTA_TIMEOUT_SEC = 40
OTA_WAIT_SEC = 900
BUILD_DIR = os.path.expanduser("~/Documents/Arduino/bike_mate/build/esp32.esp32.esp32c3")
BUILD_BIN = os.path.join(BUILD_DIR, "bike_mate.ino.bin")
CONFIG_H  = os.path.expanduser("~/Documents/Arduino/bike_mate/Config.h")

DRIVE_DIR = os.path.expanduser("~/bike-mate-drive")
DRIVE_PORT = 8001

BACKUP_DELAY_SEC = 600

WEATHER_LAT = -27.28
WEATHER_LON = 152.51
WEATHER_TZ  = "Australia%2FBrisbane"
WEATHER_REFRESH_SEC = 1800
PULL_TIMEOUT_SEC = 35

BG, CARD, GRID = "#1e1e2e", "#313244", "#2a2a3a"
FG, BLUE, GREEN, YELLOW, ORANGE, MUTED = (
    "#cdd6f4", "#89b4fa", "#a6e3a1", "#f9e2af", "#fab387", "#9399b2")
RED = "#e64553"
FILL_GREEN = "#2a4a35"
FILL_BLUE  = "#2c3f5e"

HIST_LEN = 60
volt_hist = deque([None] * HIST_LEN, maxlen=HIST_LEN)
temp_hist = deque([None] * HIST_LEN, maxlen=HIST_LEN)
latest_data = {"v": 12.6, "t": 20.0, "a": 0, "e": 0, "w": 0, "s": "MONITOR", "p": 0, "fv": "?"}
weather = {"temp": 0.0, "desc": "Loading...", "emoji": "⏳", "updated": 0}
latest_ride = None
latest_ride_lock = threading.RLock()
status_msg = ""
status_lock = threading.RLock()
last_parse_fail = 0
device_version = None
ota_server_proc = None
drive_server_proc = None

SSL_CTX = ssl._create_unverified_context()


def get_lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip


def start_ota_server():
    global ota_server_proc
    if ota_server_proc is not None:
        return ota_server_proc
    try:
        if os.path.isdir(OTA_DIR):
            shutil.rmtree(OTA_DIR)
        os.makedirs(OTA_DIR, exist_ok=True)
        ota_server_proc = subprocess.Popen(
            ["python3", "-m", "http.server", str(OTA_PORT),
             "--directory", OTA_DIR, "--bind", "0.0.0.0"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f"[OTA] server started on port {OTA_PORT}, dir {OTA_DIR}")
    except Exception as e:
        print(f"[OTA] server start failed: {e}")
    return ota_server_proc


def stop_ota_server():
    global ota_server_proc
    if ota_server_proc:
        try:
            ota_server_proc.terminate()
        except Exception:
            pass
        ota_server_proc = None


def start_drive_server():
    global drive_server_proc
    if drive_server_proc is not None:
        return drive_server_proc
    try:
        os.makedirs(DRIVE_DIR, exist_ok=True)
        for sub in ("wakes", "rides", "other"):
            os.makedirs(os.path.join(DRIVE_DIR, sub), exist_ok=True)
        drive_server_proc = subprocess.Popen(
            ["python3", "-m", "http.server", str(DRIVE_PORT),
             "--directory", DRIVE_DIR, "--bind", "0.0.0.0"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f"[DRIVE] server started on port {DRIVE_PORT}, dir {DRIVE_DIR}")
    except Exception as e:
        print(f"[DRIVE] server start failed: {e}")
    return drive_server_proc


def stop_drive_server():
    global drive_server_proc
    if drive_server_proc:
        try:
            drive_server_proc.terminate()
        except Exception:
            pass
        drive_server_proc = None


def read_firmware_version():
    try:
        with open(CONFIG_H) as f:
            content = f.read()
        m = re.search(r'#define\s+BIKE_MATE_VERSION\s+"([^"]+)"', content)
        return m.group(1) if m else None
    except Exception as e:
        print(f"[OTA] read version failed: {e}")
        return None


def compute_md5(path):
    h = hashlib.md5()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        return h.hexdigest()
    except Exception as e:
        print(f"[OTA] md5 failed: {e}")
        return None


def fetch_drive_list():
    try:
        r = urllib.request.urlopen(UPLOAD_URL + "?action=list",
                                   timeout=30, context=SSL_CTX)
        return json.loads(r.read().decode())
    except Exception as e:
        print(f"[SYNC] list failed: {e}")
        return None


def categorize(name):
    if name.startswith("wakes_"):
        return "wakes"
    if name.startswith("ride_"):
        return "rides"
    return "other"


def sync_from_drive():
    files = fetch_drive_list()
    if files is None:
        return -1, 0

    local = set()
    for sub in ("wakes", "rides", "other"):
        d = os.path.join(DRIVE_DIR, sub)
        if os.path.isdir(d):
            for n in os.listdir(d):
                local.add(n)

    missing = [f for f in files if f.get("name") and f["name"] not in local]

    print(f"[SYNC] {len(files)} in Drive, {len(local)} local, {len(missing)} new")

    if len(missing) == 0:
        return 0, 0

    msg = f"{len(missing)} files to come. Sync?"
    if not messagebox.askyesno("Sync from Drive", msg):
        return -1, -1

    dl = 0
    for f in missing:
        name = f["name"]
        sub = categorize(name)
        dest = os.path.join(DRIVE_DIR, sub, name)
        url = UPLOAD_URL + "?action=download&name=" + urllib.parse.quote(name)
        try:
            r = urllib.request.urlopen(url, timeout=60, context=SSL_CTX)
            data = r.read()
            with open(dest, "wb") as fh:
                fh.write(data)
            dl += 1
            print(f"[SYNC] {dl}/{len(missing)} {name} ({len(data)} bytes)")
        except Exception as e:
            print(f"[SYNC] download failed {name}: {e}")

    print(f"[SYNC] downloaded {dl} of {len(missing)}")
    return dl, len(missing)


def backup_firmware_to_drive():
    try:
        ver = read_firmware_version() or "unknown"
        with open(BUILD_BIN, "rb") as f:
            raw = f.read()
        b64 = base64.b64encode(raw).decode()
        body = urllib.parse.urlencode({
            "filename": "bike_mate.bin",
            "folder": ver,
            "binary": "1",
            "data": b64,
        }).encode()
        req = urllib.request.Request(UPLOAD_URL, data=body,
                                     headers={"Content-Type": "application/x-www-form-urlencoded"})
        r = urllib.request.urlopen(req, timeout=120, context=SSL_CTX)
        resp = r.read().decode()
        print(f"[BACKUP] {resp[:120]}")
        return resp.startswith("OK")
    except Exception as e:
        print(f"[BACKUP] failed: {e}")
        return False


def local_time_str():
    n = datetime.now()
    h = n.hour % 12 or 12
    return f"{h}:{n.minute:02d}{'am' if n.hour < 12 else 'pm'}"


def time_emoji():
    h = datetime.now().hour
    return ("🌙" if h < 5 else "🌅" if h < 7 else "🌄" if h < 10 else "☀️" if h < 12
            else "🌞" if h < 14 else "🌤" if h < 17 else "🌇" if h < 19
            else "🌆" if h < 21 else "🌙")


def weather_code_to_emoji(code, is_day):
    if code == 0:
        return "☀️" if is_day else "🌙"
    return {
        1: "🌤", 2: "⛅", 3: "☁️",
        45: "🌫", 48: "🌫",
        51: "🌦", 53: "🌦", 55: "🌦",
        61: "🌧", 63: "🌧", 65: "🌧",
        71: "🌨", 73: "🌨", 75: "🌨",
        80: "🌦", 81: "🌧", 82: "⛈",
        95: "⛈", 96: "⛈", 99: "⛈",
    }.get(code, "❓")


def fetch_weather():
    global weather
    url = (
        f"http://api.open-meteo.com/v1/forecast?"
        f"latitude={WEATHER_LAT}&longitude={WEATHER_LON}"
        f"&current=temperature_2m,weather_code,is_day"
        f"&timezone={WEATHER_TZ}"
    )
    try:
        with urllib.request.urlopen(url, timeout=8) as r:
            data = json.loads(r.read().decode())
        cur = data["current"]
        weather["temp"] = float(cur.get("temperature_2m", 0.0))
        weather["emoji"] = weather_code_to_emoji(
            int(cur.get("weather_code", -1)), int(cur.get("is_day", 1))
        )
        weather["updated"] = time.time()
        print(f"[WEATHER] {weather['temp']:.1f}C {weather['emoji']}")
    except Exception as e:
        print(f"[WEATHER] fetch failed: {e}")


def weather_thread_loop():
    while True:
        fetch_weather()
        time.sleep(WEATHER_REFRESH_SEC)


def set_status(m):
    global status_msg
    with status_lock:
        status_msg = m


def get_status():
    with status_lock:
        return status_msg


def ago_str(e):
    if e == 0:
        return "never"
    d = time.time() - e
    if d < 60: return f"{int(d)}s ago"
    if d < 3600: return f"{int(d/60)}m ago"
    if d < 86400: return f"{int(d/3600)}h ago"
    return f"{int(d/86400)}d ago"


def parse_summary(data):
    if len(data) < 48:
        return None
    s = struct.unpack("<IIIHHHbbHHBBHiiiiHH", bytes(data[:48]))
    return {
        "startEpoch":    s[0],
        "endEpoch":      s[1],
        "durationSecs":  s[2],
        "minVolt":       s[3] / 100.0,
        "maxVolt":       s[4] / 100.0,
        "avgVolt":       s[5] / 100.0,
        "minTemp":       s[6],
        "maxTemp":       s[7],
        "underSecs":     s[8],
        "overSecs":      s[9],
        "flags":         s[10],
        "rowCount_hi":   s[11],
        "preRideVolt":   s[12] / 100.0,
        "startLat_x1e7": s[13],
        "startLon_x1e7": s[14],
        "endLat_x1e7":   s[15],
        "endLon_x1e7":   s[16],
        "rowCount":      s[17],
    }


def parse_row(data):
    if len(data) < 8:
        return None
    r = struct.unpack("<IHbB", bytes(data[:8]))
    return {"epoch": r[0], "volt": r[1] / 100.0, "temp": r[2], "state": r[3]}


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

        global latest_ride
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
                with latest_ride_lock:
                    latest_ride = newest
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
        global latest_data, last_parse_fail, device_version

        def on_data(sender, data):
            global latest_data, last_parse_fail, device_version
            payload = bytes(data)

            if not payload:
                return
            try:
                d = json.loads(payload.decode())
                latest_data.update(d)
                if "fv" in d:
                    if device_version != d["fv"]:
                        device_version = d["fv"]
                        print(f"[DEVICE] firmware version: {d['fv']}")
                volt_hist.append(float(d.get("v", 12.6)))
                temp_hist.append(float(d.get("t", 20.0)))
            except Exception as e:
                if time.time() - last_parse_fail > 10:
                    last_parse_fail = time.time()
                    print(f"[NOTIFY] parse fail: {e}")

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
                            st = latest_data.get("s", "MONITOR")
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


class Graph(tk.Canvas):
    def __init__(self, parent, color, fill, y_min=None, y_max=None, w=380, h=120):
        super().__init__(parent, width=w, height=h, bg=BG, highlightthickness=0)
        self.color, self.fill, self.y_min, self.y_max = color, fill, y_min, y_max
        self.w, self.h = w, h
        self.pad_l, self.pad_r, self.pad_t, self.pad_b = 34, 6, 6, 6
        self.data = deque([None] * HIST_LEN, maxlen=HIST_LEN)

    def set_data(self, d):
        self.data = d
        self.redraw()

    def redraw(self):
        self.delete("all")
        pts = [p for p in self.data if p is not None]
        pw = self.w - self.pad_l - self.pad_r
        ph = self.h - self.pad_t - self.pad_b
        if self.y_min is not None and self.y_max is not None:
            lo, hi = self.y_min, self.y_max
        elif pts:
            lo, hi = min(pts), max(pts)
            if hi - lo < 2:
                m = (hi + lo) / 2
                lo, hi = m - 1, m + 1
            lo -= 0.5; hi += 0.5
        else:
            lo, hi = -1, 1
        rng = (hi - lo) or 1
        for i in range(5):
            v = hi - (i / 4) * rng
            y = self.pad_t + (i / 4) * ph
            self.create_line(self.pad_l, y, self.w - self.pad_r, y, fill=GRID)
            self.create_text(self.pad_l - 4, y, text=f"{v:.1f}", fill=MUTED,
                             font=("Helvetica", 9), anchor="e")
        if not pts:
            self.create_text(self.w / 2, self.h / 2, text="waiting…", fill=MUTED)
            return
        first = next((i for i, p in enumerate(self.data) if p is not None), 0)
        trim = [p for p in list(self.data)[first:] if p is not None]
        m = len(trim)
        if m < 1: return
        coords = []
        for i, v in enumerate(trim):
            x = self.pad_l + pw if m == 1 else self.pad_l + (i / (m - 1)) * pw
            y = self.pad_t + ph - ((v - lo) / rng) * ph
            coords.extend([x, y])
        if m >= 2:
            poly = [self.pad_l, self.pad_t + ph] + coords + \
                   [self.pad_l + pw, self.pad_t + ph]
            self.create_polygon(poly, fill=self.fill, outline="")
            self.create_line(*coords, fill=self.color, width=2,
                             capstyle=tk.ROUND, joinstyle=tk.ROUND)
        lx, ly = coords[-2], coords[-1]
        self.create_oval(lx - 3, ly - 3, lx + 3, ly + 3,
                         fill=self.color, outline="")


class App:
    def __init__(self, root):
        self.root = root
        root.title("Bike-Mate")
        root.configure(bg=BG)
        root.resizable(False, False)
        start_ota_server()
        start_drive_server()
        self.worker = BLEWorker()
        mb = tk.Menu(root)
        am = tk.Menu(mb, tearoff=0)
        am.add_command(label="About")
        am.add_separator()
        am.add_command(label="☁️  Open Cloud Drive", command=self.open_drive)
        am.add_command(label="💾  Open Local Drive", command=self.open_local_drive)
        am.add_command(label="⬇️  Sync from Drive", command=self.menu_sync)
        am.add_separator()
        am.add_command(label="📤 Update Firmware", command=self.menu_ota)
        am.add_separator()
        am.add_command(label="📊 Open Reports", command=self.open_reports)
        am.add_separator()
        am.add_command(label="Quit", command=self.on_quit)
        mb.add_cascade(label="🏍️ Bike-Mate", menu=am)
        root.config(menu=mb)
        tk.Label(root, text="🏍️ Bike-Mate", bg=BG, fg=BLUE,
                 font=("Helvetica", 26, "bold")).pack(pady=(20, 4))
        self.weather_label = tk.Label(root, text="Atkinsons Dam · --.-° ⏳",
                                      bg=BG, fg=FG,
                                      font=("Helvetica", 20, "bold"))
        self.weather_label.pack(pady=(0, 12))
        bc = tk.Frame(root, bg=CARD); bc.pack(fill="x", padx=20, pady=6)
        br = tk.Frame(bc, bg=CARD); br.pack(fill="x", pady=10)
        self.acc_lbl = self._col(br, "ACC", "--")
        self.eng_lbl = self._col(br, "Engine", "--")
        self.warn_lbl = self._col(br, "Warn", "--")
        sf = tk.Frame(root, bg=CARD); sf.pack(fill="x", padx=20, pady=6)
        self.state_lbl = tk.Label(sf, text="💤 MONITOR", bg=CARD, fg=BLUE,
                                  font=("Helvetica", 26, "bold"))
        self.state_lbl.pack(pady=14)
        vc = tk.Frame(root, bg=CARD); vc.pack(fill="x", padx=20, pady=6)
        self.volt_lbl = tk.Label(vc, text="--.-- V", bg=CARD, fg=GREEN,
                                 font=("Helvetica", 46, "bold"))
        self.volt_lbl.pack(pady=14)
        rc = tk.Frame(root, bg=CARD); rc.pack(fill="x", padx=20, pady=6)
        ri = tk.Frame(rc, bg=CARD); ri.pack(fill="x", pady=6)
        self.temp_lbl = self._col(ri, "Temp", "--.-C")
        self.time_lbl = self._col(ri, "Time", "--:--")
        lr = tk.Frame(root, bg=CARD); lr.pack(fill="x", padx=20, pady=6)
        tk.Label(lr, text="Last Ride", bg=CARD, fg=MUTED,
                 font=("Helvetica", 10)).pack(pady=(8, 2))
        self.last_ride_lbl = tk.Label(lr, text="none yet", bg=CARD, fg=MUTED,
                                      font=("Helvetica", 13, "bold"))
        self.last_ride_lbl.pack()
        self.last_ride_sub = tk.Label(lr, text="", bg=CARD, fg=MUTED,
                                      font=("Helvetica", 10))
        self.last_ride_sub.pack(pady=(2, 0))
        self.last_ride_pre = tk.Label(lr, text="", bg=CARD, fg=MUTED,
                                      font=("Helvetica", 11, "bold"))
        self.last_ride_pre.pack(pady=(0, 8))
        vgc = tk.Frame(root, bg=CARD); vgc.pack(fill="x", padx=20, pady=6)
        self.volt_graph = Graph(vgc, GREEN, FILL_GREEN, y_min=11.5, y_max=15.0)
        self.volt_graph.pack(padx=8, pady=8)
        tgc = tk.Frame(root, bg=CARD); tgc.pack(fill="x", padx=20, pady=6)
        self.temp_graph = Graph(tgc, BLUE, FILL_BLUE)
        self.temp_graph.pack(padx=8, pady=8)
        self.footer_lbl = tk.Label(root, text=f"GUI v{GUI_VERSION}",
                                   bg=BG, fg=MUTED,
                                   font=("Helvetica", 9))
        self.footer_lbl.pack(side="bottom", pady=6)
        root.protocol("WM_DELETE_WINDOW", self.on_quit)
        threading.Thread(target=weather_thread_loop, daemon=True).start()
        self.worker.start()
        self.tick()

    def _col(self, parent, label, value):
        c = tk.Frame(parent, bg=CARD); c.pack(side="left", expand=True, fill="x")
        tk.Label(c, text=label, bg=CARD, fg=MUTED,
                 font=("Helvetica", 11)).pack(pady=(4, 2))
        v = tk.Label(c, text=value, bg=CARD, fg=FG,
                     font=("Helvetica", 16, "bold"))
        v.pack(pady=(0, 4))
        return v

    def on_quit(self):
        stop_ota_server()
        stop_drive_server()
        self.root.destroy()

    def open_drive(self):
        os.system(f'open "{DRIVE_FOLDER_URL}"')

    def open_local_drive(self):
        os.makedirs(DRIVE_DIR, exist_ok=True)
        os.system(f'open "{DRIVE_DIR}"')

    def open_reports(self):
        messagebox.showinfo("Reports", "TBA")

    def menu_sync(self):
        def _worker():
            set_status("syncing...")
            dl, total = sync_from_drive()
            set_status("")
            if dl < 0 and total < 0:
                return
            if dl == 0:
                self.root.after(0, lambda: messagebox.showinfo(
                    "Sync", "No new files"))
            elif dl > 0:
                self.root.after(0, lambda: messagebox.showinfo(
                    "Sync", f"Sync complete: {dl} files"))
            else:
                self.root.after(0, lambda: messagebox.showerror(
                    "Sync", "Sync failed"))
        threading.Thread(target=_worker, daemon=True).start()

    def menu_ota(self):
        if not os.path.isfile(BUILD_BIN):
            messagebox.showerror("OTA", f"Firmware not found:\n{BUILD_BIN}\n\nRun Sketch → Export Compiled Binary in Arduino IDE first.")
            return

        src_ver = read_firmware_version() or "?"

        try:
            bin_mtime = os.path.getmtime(BUILD_BIN)
            cfg_mtime = os.path.getmtime(CONFIG_H)
            stale = cfg_mtime > bin_mtime
        except Exception:
            stale = False
            bin_mtime = 0

        size = os.path.getsize(BUILD_BIN)
        md5 = compute_md5(BUILD_BIN) or "?"
        dev_ver = latest_data.get("fv", "?")

        built_str = datetime.fromtimestamp(bin_mtime).strftime("%Y-%m-%d %H:%M") if bin_mtime else "?"
        size_mb = size / (1024 * 1024)
        msg = (
            f"File:        bike_mate.ino.bin\n"
            f"Size:        {size:,} bytes ({size_mb:.2f} MB)\n"
            f"MD5:         {md5[:16]}...\n"
            f"Source ver:  {src_ver}\n"
            f"Built:       {built_str}\n"
            f"Device ver:  {dev_ver}\n"
        )
        if stale:
            msg += "\n⚠️  Config.h is newer than the .bin.\nRe-export the binary before updating."
        msg += "\n\nProceed with OTA update?"

        if not messagebox.askyesno("Bike-Mate OTA", msg):
            return

        dest = os.path.join(OTA_DIR, "bike_mate.bin")
        try:
            shutil.copy(BUILD_BIN, dest)
            print(f"[OTA] copied to {dest}")
        except Exception as e:
            messagebox.showerror("OTA", f"Copy failed:\n{e}")
            return

        def _backup():
            print(f"[BACKUP] waiting {BACKUP_DELAY_SEC}s before upload")
            time.sleep(BACKUP_DELAY_SEC)
            ok = backup_firmware_to_drive()
            if ok:
                print(f"[BACKUP] firmware {src_ver} backed up to Drive")
            else:
                print(f"[BACKUP] failed for {src_ver}")

        threading.Thread(target=_backup, daemon=True).start()

        url = f"http://{OTA_HOSTNAME}.local:{OTA_PORT}/bike_mate.bin"
        payload = json.dumps(
            {"url": url, "size": size, "md5": md5, "ver": src_ver},
            separators=(",", ":"))
        print(f"[OTA] payload: {payload}")

        set_status("sending OTA command...")

        def on_result(ok, detail):
            if ok:
                set_status(f"OTA metadata sent - device will fetch from {url}")
                self.root.after(0, lambda: messagebox.showinfo(
                    "OTA",
                    f"Device acknowledged.\n\n"
                    f"The device will fetch and flash on its next wake.\n"
                    f"Confirmation email will arrive once complete.\n\n"
                    f"URL: {url}"))
            else:
                try:
                    os.remove(dest)
                    print("[OTA] cleaned up staged file (BLE failed)")
                except Exception as e:
                    print(f"[OTA] cleanup failed: {e}")
                set_status(f"OTA failed: {detail}")
                self.root.after(0, lambda: messagebox.showerror(
                    "OTA",
                    f"Device did not acknowledge.\n\n"
                    f"Reason: {detail}\n\n"
                    f"Staged file removed."))

        self.worker.send_ota_command(payload, on_result)

    def tick(self):
        d = latest_data
        v = d.get("v", 12.6); t = d.get("t", 20.0); st = d.get("s", "MONITOR")
        if v >= 13.8: c, ve = GREEN, "⚡"
        elif v >= 12.5: c, ve = GREEN, "🔋"
        elif v >= 12.0: c, ve = YELLOW, "⚠️"
        else: c, ve = RED, "🆘"
        self.volt_lbl.config(text=f"{ve} {v:.2f} V", fg=c)
        em = {"RUNNING": "🟢", "MONITOR": "💤", "SLEEP": "💤", "PANIC": "🚨"}.get(st, "")
        self.state_lbl.config(text=f"{em} {st}")
        if st == "RUNNING": self.state_lbl.config(fg=GREEN)
        elif st == "PANIC": self.state_lbl.config(fg=RED)
        elif st == "MONITOR": self.state_lbl.config(fg=BLUE)
        else: self.state_lbl.config(fg=FG)
        if t < 0: te = "❄️"
        elif t < 10: te = "🥶"
        elif t < 20: te = "🌤"
        elif t < 30: te = "☀️"
        elif t < 40: te = "🔥"
        else: te = "💀"
        self.temp_lbl.config(text=f"{te} {t:.1f}C")
        self.time_lbl.config(text=f"{time_emoji()} {local_time_str()}")
        self.acc_lbl.config(text="ON" if d.get("a") else "OFF",
                            fg=GREEN if d.get("a") else MUTED)
        self.eng_lbl.config(text="RUN" if d.get("e") else "PARK",
                            fg=GREEN if d.get("e") else MUTED)
        self.warn_lbl.config(text="YES" if d.get("w") else "NO",
                             fg=RED if d.get("w") else MUTED)
        self.weather_label.config(
            text=f"Atkinsons Dam · {weather['temp']:.1f}° {weather['emoji']}")
        fw = latest_data.get("fv", "?")
        self.footer_lbl.config(text=f"GUI v{GUI_VERSION}  ·  FW v{fw}")
        with latest_ride_lock:
            r = latest_ride
        if r and r["summary"]:
            s = r["summary"]
            start = datetime.fromtimestamp(s["startEpoch"])
            self.last_ride_lbl.config(text=start.strftime("%a %d %b, %I:%M%p"), fg=FG)
            if s["flags"] == 0:
                sub_color = GREEN
            elif s["flags"] & 0x30 or s["flags"] & 0x08:
                sub_color = RED
            else:
                sub_color = YELLOW
            self.last_ride_sub.config(
                text=f"{ago_str(s['startEpoch'])}  ·  {s['durationSecs']//60} min",
                fg=sub_color)
            pre = s.get("preRideVolt", 0)
            if pre > 0:
                self.last_ride_pre.config(
                    text=f"cold {pre:.2f}V  →  ride {s['avgVolt']:.2f}V",
                    fg=YELLOW)
            else:
                self.last_ride_pre.config(text="", fg=MUTED)
        else:
            self.last_ride_lbl.config(text="none yet", fg=MUTED)
            self.last_ride_sub.config(text="", fg=MUTED)
            self.last_ride_pre.config(text="", fg=MUTED)
        self.volt_graph.set_data(volt_hist)
        self.temp_graph.set_data(temp_hist)
        self.root.after(1000, self.tick)


if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()