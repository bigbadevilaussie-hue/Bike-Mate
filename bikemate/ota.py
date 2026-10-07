# === Bike-Mate GUI: ota ===
# auto-extracted, edit here ===

import os, subprocess, time, hashlib, shutil
from datetime import datetime
import requests
from .config import *
from . import state
from .helpers import *


ota_server_proc = None


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

