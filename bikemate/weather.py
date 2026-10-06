# === Bike-Mate GUI: weather ===
# auto-extracted, edit here ===

import json, time, urllib.request
from .config import *
from . import state


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
        state.weather["temp"] = float(cur.get("temperature_2m", 0.0))
        state.weather["emoji"] = weather_code_to_emoji(
            int(cur.get("weather_code", -1)), int(cur.get("is_day", 1))
        )
        state.weather["updated"] = time.time()
        print(f"[WEATHER] {state.weather['temp']:.1f}C {state.weather['emoji']}")
    except Exception as e:
        print(f"[WEATHER] fetch failed: {e}")


def weather_thread_loop():
    while True:
        fetch_weather()
        time.sleep(WEATHER_REFRESH_SEC)

