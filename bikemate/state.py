# === Bike-Mate GUI: state ===
# auto-extracted, edit here ===

import threading
import ssl
from collections import deque
from .config import HIST_LEN

# Maintenance mode state. "OFF" | "PENDING" | "ON"
# Firmware doesn't implement the command yet, so PENDING will
# currently time out back to OFF. Once firmware ACKs, PENDING
# flips to ON and stays until "off" ACK.
maintenance_state = "OFF"
maintenance_lock = threading.RLock()
maint_pending_since = 0.0
maint_pending_timeout = 0
temp_hist = deque([None] * HIST_LEN, maxlen=HIST_LEN)
volt_hist = deque([None] * HIST_LEN, maxlen=HIST_LEN)
latest_data = {"v": None, "t": None, "a": 0, "e": 0, "w": 0, "s": "--", "p": 0, "fv": "?"}
latest_seen_time = 0.0
weather = {"temp": 0.0, "desc": "Loading...", "emoji": "⏳", "updated": 0}
latest_ride = None
latest_ride_lock = threading.RLock()
status_msg = ""
status_lock = threading.RLock()
last_parse_fail = 0
device_version = None

SSL_CTX = ssl._create_unverified_context()

