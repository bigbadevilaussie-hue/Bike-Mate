# === Bike-Mate GUI: config ===
# auto-extracted, edit here ===

import os
import threading

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
import requests
from collections import deque
from datetime import datetime
from tkinter import messagebox
from bleak import BleakScanner, BleakClient

GUI_VERSION = "4.33"

MAINTENANCE_ACK_TIMEOUT_MS = 5000
MAINTENANCE_BROWSER_DELAY_MS = 3000
BIKE_IP = "192.168.8.196"
DEVICE_NAME = "Bike-Mate-2"
DATA_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
TIME_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9"
STREAM_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ab"
REQUEST_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ad"
OTA_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ae"
SETTINGS_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ae"  # reuse OTA char to bypass macOS cache

DRIVE_FOLDER_URL = "https://drive.google.com/drive/folders/1I48SYu8vTC4CDTFLUhZh53siI8ULbQ8W"
UPLOAD_URL = "https://script.google.com/macros/s/AKfycbx758YfZhY4wp_FdWgu6DSoDVn6-k0Wb0hVVDzwauRSZOSbT9zFAsvskeHv4mz5-G59/exec"

HIST_LEN = 60

OTA_DIR = os.path.expanduser("~/bike-mate-ota")
# V4.34: separate archive dir. ota.py wipes OTA_DIR on GUI start
# (rmtree + makedirs), which was destroying the archived .bins.
OTA_ARCHIVE_DIR = os.path.expanduser("~/bike-mate-ota-archive")
OTA_PORT = 8000
OTA_HOSTNAME = "192.168.8.187"
OTA_TIMEOUT_SEC = 40
OTA_WAIT_SEC = 900
BUILD_DIR = os.path.expanduser("~/Documents/Arduino/bike_mate/build/esp32.esp32.esp32c3")
BUILD_BIN = os.path.join(BUILD_DIR, "bike_mate.ino.bin")
CONFIG_H  = os.path.expanduser("~/Documents/Arduino/bike_mate/Config.h")

DRIVE_DIR = os.path.expanduser("~/bike-mate-drive")
DRIVE_PORT = 8001

BACKUP_DELAY_SEC = 600

WEATHER_LAT = -27.4319
WEATHER_LON = 152.4511
WEATHER_TZ  = "Australia%2FBrisbane"
WEATHER_REFRESH_SEC = 1800
PULL_TIMEOUT_SEC = 35
BLE_STALE_SEC = 60

THEME_DAY = {
    "bg": "#eef1f7", "card": "#ffffff", "card_border": "#d8dde8",
    "grid": "#e6e9f0", "fg": "#1e1e2e", "muted": "#7a8194",
    "accent": "#1e66f5", "blue": "#1e66f5", "green": "#2f9e44",
    "yellow": "#d99a00", "orange": "#e8590c", "red": "#d20f39",
    "fill": "#cfe0ff",
}
THEME_NIGHT = {
    "bg": "#181825", "card": "#232334", "card_border": "#313145",
    "grid": "#2a2a3a", "fg": "#cdd6f4", "muted": "#9399b2",
    "accent": "#89b4fa", "blue": "#89b4fa", "green": "#a6e3a1",
    "yellow": "#f9e2af", "orange": "#fab387", "red": "#e64553",
    "fill": "#2c3f5e",
}
DAY_START_HOUR = 6
DAY_END_HOUR = 18

# Firmware's wake-interval boundaries (Config.h NIGHT_START_HOUR/NIGHT_END_HOUR).
# Different from theme boundaries — the bike switches to slow wakes at 22:00,
# the GUI switches to dark theme at 18:00.
BIKE_NIGHT_START_HOUR = 22
BIKE_NIGHT_END_HOUR = 4

# Legacy aliases (night theme) - kept for code paths not yet converted.
BG, CARD, GRID = THEME_NIGHT["bg"], THEME_NIGHT["card"], THEME_NIGHT["grid"]
FG, BLUE, GREEN, YELLOW, ORANGE, MUTED = (
    THEME_NIGHT["fg"], THEME_NIGHT["blue"], THEME_NIGHT["green"],
    THEME_NIGHT["yellow"], THEME_NIGHT["orange"], THEME_NIGHT["muted"])
RED = THEME_NIGHT["red"]
FILL_GREEN = THEME_NIGHT["fill"]
FILL_BLUE  = THEME_NIGHT["fill"]

