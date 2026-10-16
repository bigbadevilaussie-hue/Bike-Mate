# === Bike-Mate GUI: ride picker ===
# Dropdown of rides from the last 24 hours, newest first.
# On select: stats board + V/T graphs.
# Reads from OSA via /bike/files/list and /bike/files.

import os, gzip, math, urllib.request, io, csv
import tkinter as tk
from datetime import datetime, date, timedelta

from .config import OSA_URL, BG, CARD, FG, MUTED, BLUE, GREEN, ORANGE, RED, GRID
from . import state
from .helpers import *


def _haversine(lat1, lon1, lat2, lon2):
    R = 6371000.0
    p1 = math.radians(lat1); p2 = math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a = (math.sin(dp / 2) ** 2 +
         math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2)
    return 2 * R * math.asin(math.sqrt(a))


def _osa_list_rides():
    try:
        url = f"{OSA_URL}/bike/files/list?dir=rides"
        with urllib.request.urlopen(url, timeout=5) as r:
            import json as _json
            d = _json.loads(r.read().decode())
        return d.get("files", [])
    except Exception as e:
        print(f"[PICKER] list failed: {e}")
        return None


def _fetch_ride(name):
    try:
        url = f"{OSA_URL}/bike/files?dir=rides&name={name}"
        with urllib.request.urlopen(url, timeout=10) as r:
            raw = r.read()
        if raw[:2] == b"\x1f\x8b":
            text = gzip.decompress(raw).decode("utf-8", "replace")
        else:
            text = raw.decode("utf-8", "replace")
        return text
    except Exception as e:
        print(f"[PICKER] fetch failed {name}: {e}")
        return None


def _parse_ride(text):
    """Ride CSV: epoch,lat,lon,volt,temp,state[,speed,alt,course]"""
    rows = []
    for r in csv.reader(io.StringIO(text)):
        if not r or r[0].startswith("#") or r[0] == "epoch":
            continue
        try:
            v = float(r[3])
            if v < 5.0:
                continue
            row = {
                "epoch": int(r[0]),
                "lat":   int(r[1]) / 1e7,
                "lon":   int(r[2]) / 1e7,
                "volt":  v,
                "temp":  int(r[4]),
            }
            if len(r) >= 9:
                row["speed"]  = float(r[6])
                row["alt"]    = int(r[7])
                row["course"] = float(r[8])
            else:
                row["speed"] = row["alt"] = row["course"] = None
            rows.append(row)
        except (ValueError, IndexError):
            continue
    return rows


def _stats(rows):
    if not rows:
        return None
    epochs = [r["epoch"] for r in rows]
    volts  = [r["volt"] for r in rows]
    temps  = [r["temp"] for r in rows]
    duration = max(epochs) - min(epochs)

    dist_m = 0.0
    for a, b in zip(rows, rows[1:]):
        if a["lat"] == 0 and a["lon"] == 0:
            continue
        if b["lat"] == 0 and b["lon"] == 0:
            continue
        dt = b["epoch"] - a["epoch"]
        if dt <= 0:
            continue
        d = _haversine(a["lat"], a["lon"], b["lat"], b["lon"])
        if d < 3 or d / dt > 60:
            continue
        dist_m += d

    gps_speeds = [r["speed"] for r in rows if r.get("speed") is not None]
    alts = [r["alt"] for r in rows if r.get("alt") is not None]

    return {
        "samples":    len(rows),
        "duration_s": duration,
        "distance_m": dist_m,
        "avg_speed":  (dist_m / duration * 3.6) if duration > 0 else 0.0,
        "max_speed":  max(gps_speeds) if gps_speeds else 0.0,
        "min_volt":   min(volts),
        "max_volt":   max(volts),
        "avg_volt":   sum(volts) / len(volts),
        "min_temp":   min(temps),
        "max_temp":   max(temps),
        "min_alt":    min(alts) if alts else None,
        "max_alt":    max(alts) if alts else None,
        "start":      datetime.fromtimestamp(min(epochs)),
        "end":        datetime.fromtimestamp(max(epochs)),
    }


class RidePicker(tk.Toplevel):
    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app
        self.title("Last Day of Rides")
        self.configure(bg=BG)
        self.resizable(False, False)
        self.transient(parent)
        self.rides = []
        self.rows_by_name = {}
        self._build()

    def _build(self):
        files = _osa_list_rides()
        if files is None:
            tk.Label(self, text="OSA unreachable.", bg=BG, fg=FG,
                     font=("Helvetica", 13), padx=30, pady=30).pack()
            tk.Button(self, text="Close", command=self.destroy).pack(pady=(0, 20))
            return

        # filter to last 24h, by filename date
        cutoff = date.today() - timedelta(days=1)
        recent = []
        for f in files:
            n = f["name"]
            if not n.startswith("ride_"):
                continue
            try:
                y = int(n[5:9]); mo = int(n[9:11]); d = int(n[11:13])
                fd = date(y, mo, d)
            except Exception:
                continue
            if fd >= cutoff:
                recent.append(f)
        self.rides = sorted(recent, key=lambda f: f["name"], reverse=True)

        if not self.rides:
            tk.Label(self, text="No rides in the last 24 hours.",
                     bg=BG, fg=FG, font=("Helvetica", 13),
                     padx=30, pady=30).pack()
            tk.Button(self, text="Close", command=self.destroy).pack(pady=(0, 20))
            return

        top = tk.Frame(self, bg=BG); top.pack(fill="x", padx=20, pady=(16, 8))
        tk.Label(top, text="Ride:", bg=BG, fg=FG,
                 font=("Helvetica", 11, "bold")).pack(side="left")
        self.var = tk.StringVar(value=self.rides[0]["name"])
        opt = tk.OptionMenu(top, self.var, *[f["name"] for f in self.rides],
                            command=self._on_select)
        opt.configure(bg=CARD, fg=FG, font=("Helvetica", 11),
                      activebackground=CARD, highlightthickness=0)
        opt.pack(side="left", padx=(6, 0))

        self.body = tk.Frame(self, bg=BG)
        self.body.pack(fill="both", expand=True, padx=20, pady=(0, 8))

        tk.Button(self, text="Close", command=self.destroy,
                  font=("Helvetica", 11)).pack(pady=(0, 16))

        self._on_select(self.rides[0]["name"])

    def _on_select(self, name):
        for w in self.body.winfo_children():
            w.destroy()
        text = _fetch_ride(name)
        if text is None:
            tk.Label(self.body, text="Fetch failed.", bg=BG, fg=RED).pack()
            return
        rows = _parse_ride(text)
        s = _stats(rows)
        if s is None:
            tk.Label(self.body, text="No data rows.", bg=BG, fg=FG).pack()
            return

        hdr = tk.Frame(self.body, bg=CARD)
        hdr.pack(fill="x", pady=(0, 6))
        tk.Label(hdr,
                 text=f"{s['start'].strftime('%a %d %b %H:%M')} → {s['end'].strftime('%H:%M')}",
                 bg=CARD, fg=BLUE, font=("Helvetica", 12, "bold")).pack(anchor="w", padx=12, pady=(10, 2))
        tk.Label(hdr, text=f"{s['samples']} samples", bg=CARD, fg=MUTED,
                 font=("Helvetica", 10)).pack(anchor="w", padx=12)

        # stats grid
        stats_frame = tk.Frame(self.body, bg=CARD)
        stats_frame.pack(fill="x", pady=(0, 10))

        def _stat(label, value):
            c = tk.Frame(stats_frame, bg=CARD)
            c.pack(side="left", expand=True, fill="x", padx=6, pady=6)
            tk.Label(c, text=label, bg=CARD, fg=MUTED,
                     font=("Helvetica", 9)).pack()
            tk.Label(c, text=value, bg=CARD, fg=FG,
                     font=("Helvetica", 14, "bold")).pack()

        dur_min = s["duration_s"] / 60
        dur_str = f"{dur_min:.1f}m" if dur_min < 60 else f"{dur_min/60:.1f}h"
        _stat("Duration", dur_str)
        _stat("Distance", f"{s['distance_m']/1000:.2f} km")
        _stat("Avg spd", f"{s['avg_speed']:.1f}")
        _stat("Max spd", f"{s['max_speed']:.1f}")
        if s["min_alt"] is not None:
            _stat("Alt", f"{s['min_alt']}…{s['max_alt']} m")
        _stat("Volt", f"{s['min_volt']:.1f}-{s['max_volt']:.1f}")

        tk.Button(self.body, text="(graphs go here)",
                  bg=CARD, fg=MUTED, state="disabled").pack(pady=6)
