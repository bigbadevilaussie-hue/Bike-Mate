# === Bike-Mate GUI: helpers ===
# auto-extracted, edit here ===

import os, re, socket, hashlib
from datetime import datetime
import requests
from .config import *
from . import state


def is_daytime():
    return DAY_START_HOUR <= datetime.now().hour < DAY_END_HOUR


def current_theme():
    return THEME_DAY if is_daytime() else THEME_NIGHT


def is_bike_daytime():
    """True if the bike is in its day wake-interval window right now.
    Uses the firmware's boundaries (22:00-04:00 is night), not the
    GUI theme boundaries (18:00-06:00 is night)."""
    hr = datetime.now().hour
    if BIKE_NIGHT_START_HOUR > BIKE_NIGHT_END_HOUR:
        return not (hr >= BIKE_NIGHT_START_HOUR or hr < BIKE_NIGHT_END_HOUR)
    return not (BIKE_NIGHT_START_HOUR <= hr < BIKE_NIGHT_END_HOUR)


def expected_wake_seconds():
    """How long until the bike's next wake in field mode.
    Day: 300 s. Night: 600 s. Uses the firmware's boundaries."""
    return 300 if is_bike_daytime() else 600


def pending_timeout_seconds():
    """How long the GUI waits for the bike to enter maint.
    Wake interval + 2 min margin."""
    return expected_wake_seconds() + 120


def get_lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip


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


def local_time_str():
    n = datetime.now()
    h = n.hour % 12 or 12
    return f"{h}:{n.minute:02d}{'am' if n.hour < 12 else 'pm'}"


def time_emoji():
    h = datetime.now().hour
    return ("🌙" if h < 5 else "🌅" if h < 7 else "🌄" if h < 10 else "☀️" if h < 12
            else "🌞" if h < 14 else "🌤" if h < 17 else "🌇" if h < 19
            else "🌆" if h < 21 else "🌙")


def set_status(m):
    with state.status_lock:
        state.status_msg = m


def get_status():
    with state.status_lock:
        return state.status_msg


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

