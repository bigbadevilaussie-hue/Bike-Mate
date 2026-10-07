# === Bike-Mate GUI: reports ===
# auto-extracted, edit here ===

import os, glob, gzip
import tkinter as tk
from datetime import datetime
from .config import *
from . import state
from .helpers import *
from .widgets import *


class BikeReport(tk.Toplevel):
    """Report window — reads local wakes_*.csv, plots V and T."""

    def __init__(self, parent, app, mode="day"):
        super().__init__(parent)
        self.app = app
        self.mode = mode
        self.configure(bg=BG)
        self.resizable(False, False)
        self.transient(parent)

        if mode == "day":
            self.title("Bike-Mate Report — Last Day")
        elif mode == "ride":
            self.title("Bike-Mate Report — Last Ride")
        else:
            self.title("Bike-Mate Report — Last Week")

        files = self._find_files()
        if not files:
            tk.Label(self, text="No wake files.\nClick Sync from Drive first.",
                     bg=BG, fg=FG, font=("Helvetica", 13),
                     padx=30, pady=30).pack()
            tk.Button(self, text="Close", command=self.destroy).pack(pady=(0, 20))
            return

        rows = []
        for f in files:
            rows.extend(self._parse(f))

        if not rows:
            tk.Label(self, text="No data rows in file.",
                     bg=BG, fg=FG, font=("Helvetica", 13),
                     padx=30, pady=30).pack()
            tk.Button(self, text="Close", command=self.destroy).pack(pady=(0, 20))
            return

        self._build_ui(rows, files)

    def _find_files(self):
        import glob
        base = os.path.join(DRIVE_DIR, "wakes")
        all_files = sorted(glob.glob(os.path.join(base, "wakes_*.csv")) +
                          glob.glob(os.path.join(base, "wakes_*.csv.gz")))

        if not all_files:
            return []

        if self.mode == "2h":
            from datetime import date
            y = date.today().strftime("%Y-%m-%d")
            target = os.path.join(base, f"wakes_{y}.csv")
            if os.path.exists(target):
                return [target]
            return all_files[-1:]

        if self.mode == "day":
            from datetime import date, timedelta
            y = (date.today() - timedelta(days=1)).strftime("%Y-%m-%d")
            target = os.path.join(base, f"wakes_{y}.csv")
            if os.path.exists(target):
                return [target]
            return all_files[-1:]

        elif self.mode == "week":
            from datetime import date, timedelta
            files = []
            for i in range(7):
                d = (date.today() - timedelta(days=i+1)).strftime("%Y-%m-%d")
                p = os.path.join(base, f"wakes_{d}.csv")
                if os.path.exists(p):
                    files.append(p)
            return sorted(files) if files else all_files[-7:]

        elif self.mode == "ride":
            rides = sorted(glob.glob(os.path.join(DRIVE_DIR, "rides", "ride_*.csv")) +
                           glob.glob(os.path.join(DRIVE_DIR, "rides", "ride_*.csv.gz")))
            if not rides:
                return all_files[-1:]
            return rides[-1:]

        return []

    def _parse(self, path):
        import csv
        rows = []
        try:
            # V4.34: support .csv.gz — the firmware gzips large rides
            # and (pre-V5.12) mislabelled the wake uploads, so some
            # files on Drive are gzip bytes under a .csv name.
            # Sniff the magic bytes so both cases work.
            with open(path, "rb") as probe:
                is_gz = probe.read(2) == b"\x1f\x8b"
            opener = gzip.open if is_gz else open
            with opener(path, "rt") as f:
                for r in csv.reader(f):
                    if not r or r[0].startswith("#") or r[0] == "epoch":
                        continue
                    try:
                        rows.append({
                            "epoch": int(r[0]),
                            "volt":  float(r[3]),
                            "temp":  int(r[4]),
                        })
                    except (ValueError, IndexError):
                        continue
        except Exception as e:
            print(f"[REPORT] parse failed {path}: {e}")
        return rows

    def _build_ui(self, rows, files):
        from datetime import datetime

        epochs = [r["epoch"] for r in rows]
        volts  = [r["volt"]  for r in rows]
        temps  = [r["temp"]  for r in rows]

        t0 = min(epochs); t1 = max(epochs)
        span_h = (t1 - t0) / 3600

        header = tk.Frame(self, bg=BG)
        header.pack(fill="x", padx=20, pady=(16, 8))
        tk.Label(header, text=f"{len(rows)} samples · {span_h:.1f} h",
                 bg=BG, fg=BLUE, font=("Helvetica", 14, "bold")).pack(anchor="w")
        tk.Label(header,
                 text=f"{datetime.fromtimestamp(t0).strftime('%a %d %b %H:%M')} → {datetime.fromtimestamp(t1).strftime('%H:%M')} · {len(files)} file(s)",
                 bg=BG, fg=MUTED, font=("Helvetica", 10)).pack(anchor="w", pady=(2, 0))

        tk.Label(self, text="VOLTAGE (V)", bg=BG, fg=FG,
                 font=("Helvetica", 11, "bold")).pack(pady=(12, 4))
        c1 = tk.Canvas(self, width=640, height=160, bg=CARD, highlightthickness=0)
        c1.pack(padx=20)
        self._draw_graph(c1, epochs, volts, GREEN, FILL_GREEN,
                         f"{min(volts):.2f}-{max(volts):.2f} avg {sum(volts)/len(volts):.2f}")

        tk.Label(self, text="TEMPERATURE (C)", bg=BG, fg=FG,
                 font=("Helvetica", 11, "bold")).pack(pady=(16, 4))
        c2 = tk.Canvas(self, width=640, height=160, bg=CARD, highlightthickness=0)
        c2.pack(padx=20)
        self._draw_graph(c2, epochs, temps, BLUE, FILL_BLUE,
                         f"{min(temps)}-{max(temps)} avg {sum(temps)//len(temps)}")

        tk.Button(self, text="Close", command=self.destroy,
                  font=("Helvetica", 11)).pack(pady=(16, 20))

        # V4.43: theme the report to match the current theme.
        t = self.app.theme
        def tw(w):
            try:
                cls = w.winfo_class()
            except Exception:
                return
            try:
                if cls in ("Frame", "Toplevel"):
                    w.configure(bg=t["bg"])
                elif cls == "Label":
                    # header labels use bg, card labels use card
                    if w.master.winfo_class() in ("Frame", "Toplevel"):
                        w.configure(bg=t["bg"])
                elif cls == "Canvas":
                    w.configure(bg=t["card"])
                elif cls == "Button":
                    w.configure(bg=t["card"], fg=t["fg"],
                                activebackground=t["accent"],
                                activeforeground=t["fg"])
            except Exception:
                pass
            for c in w.winfo_children():
                tw(c)
        tw(self)

    def _draw_graph(self, canvas, xs, ys, color, fill, subtitle=""):
        W, H = 640, 160
        pad_l, pad_r, pad_t, pad_b = 50, 10, 10, 25
        pw = W - pad_l - pad_r
        ph = H - pad_t - pad_b

        x_min, x_max = min(xs), max(xs)
        y_min, y_max = min(ys), max(ys)
        if y_max - y_min < 1:
            y_min -= 0.5; y_max += 0.5
        else:
            pad = (y_max - y_min) * 0.1
            y_min -= pad; y_max += pad
        x_range = (x_max - x_min) or 1
        y_range = (y_max - y_min) or 1

        for i in range(4):
            v = y_max - (i / 3) * y_range
            y = pad_t + (i / 3) * ph
            canvas.create_line(pad_l, y, W - pad_r, y, fill=GRID)
            canvas.create_text(pad_l - 5, y, text=f"{v:.1f}",
                               fill=MUTED, font=("Helvetica", 9), anchor="e")

        from datetime import datetime
        for i in range(4):
            t = x_min + (i / 3) * x_range
            x = pad_l + (i / 3) * pw
            canvas.create_text(x, H - 8,
                               text=datetime.fromtimestamp(t).strftime("%H:%M"),
                               fill=MUTED, font=("Helvetica", 9))

        coords = []
        for t, v in zip(xs, ys):
            px = pad_l + ((t - x_min) / x_range) * pw
            py = pad_t + ph - ((v - y_min) / y_range) * ph
            coords.extend([px, py])

        if len(coords) >= 4:
            poly = [pad_l, pad_t + ph] + coords + [pad_l + pw, pad_t + ph]
            canvas.create_polygon(poly, fill=fill, outline="")
            canvas.create_line(*coords, fill=color, width=1)

        if subtitle:
            canvas.create_text(W - pad_r - 5, pad_t + 4, text=subtitle,
                               fill=MUTED, font=("Helvetica", 9), anchor="ne")




class WaitingForRide(tk.Toplevel):
    """Waiting screen - polls latest_ride up to 3 wake cycles."""

    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app
        self.title("Bike-Mate - Last Ride")
        self.resizable(False, False)
        self.transient(parent)
        self._poll_count = 0
        self._max_polls = 3
        self._poll_interval_ms = 30000

        wrap = tk.Frame(self)
        wrap.pack(padx=30, pady=30)
        self._msg = tk.Label(wrap, text="Waiting for device to wake...",
                             font=("Helvetica", 14, "bold"))
        self._msg.pack(pady=(0, 10))
        self._sub = tk.Label(wrap, text="0 of 3 cycles",
                             font=("Helvetica", 11))
        self._sub.pack(pady=(0, 20))
        tk.Button(wrap, text="Cancel", width=12,
                  command=self.destroy).pack()

        t = app.theme
        self.configure(bg=t["bg"])
        wrap.configure(bg=t["bg"])
        self._msg.configure(bg=t["bg"], fg=t["fg"])
        self._sub.configure(bg=t["bg"], fg=t["muted"])

        self.after(1000, self._poll)

    def _poll(self):
        with state.latest_ride_lock:
            r = state.latest_ride
        if r and r.get("summary"):
            self.destroy()
            LastRideReport(self.app.root, self.app)
            return
        self._poll_count += 1
        if self._poll_count >= self._max_polls:
            self._msg.config(text="No ride data received")
            self._sub.config(text="Device may be asleep")
            return
        self._sub.config(text=f"{self._poll_count} of {self._max_polls} cycles")
        self.after(self._poll_interval_ms, self._poll)


class LastRideReport(tk.Toplevel):
    """Last ride report - stat cards + voltage + temp graphs."""

    VOLT_OK_MIN = 12.2
    VOLT_OK_MAX = 14.8

    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app
        self.title("Bike-Mate - Last Ride")
        self.resizable(False, False)
        self.transient(parent)

        with state.latest_ride_lock:
            ride = state.latest_ride
        if not ride or not ride.get("summary"):
            tk.Label(self, text="No ride data", font=("Helvetica", 14)).pack(padx=40, pady=40)
            tk.Button(self, text="Close", command=self.destroy).pack(pady=(0, 20))
            return

        self._ride = ride
        self._build_ui()

    def _build_ui(self):
        t = self.app.theme
        self.configure(bg=t["bg"])

        s = self._ride["summary"]
        rows = self._ride["rows"]

        # Header
        from datetime import datetime
        start = datetime.fromtimestamp(s["startEpoch"])
        dur_min = s["durationSecs"] // 60
        dur_sec = s["durationSecs"] % 60

        header = tk.Frame(self, bg=t["bg"])
        header.pack(fill="x", padx=20, pady=(16, 8))
        tk.Label(header, text=f"Last Ride  -  {start.strftime('%a %d %b, %I:%M%p')}",
                 bg=t["bg"], fg=t["blue"], font=("Helvetica", 15, "bold")).pack(anchor="w")
        tk.Label(header, text=f"{dur_min}m {dur_sec}s  -  {s['rowCount']} rows  -  5s cadence",
                 bg=t["bg"], fg=t["muted"], font=("Helvetica", 10)).pack(anchor="w", pady=(2, 0))

        # Stat cards - voltage
        vc = tk.Frame(self, bg=t["card"]); vc.pack(fill="x", padx=20, pady=(12, 4))
        vcf = tk.Frame(vc, bg=t["card"]); vcf.pack(fill="x", pady=10)
        self._stat(vcf, "VOLT MIN", f"{s['minVolt']:.2f}V",
                   t["red"] if s["minVolt"] < self.VOLT_OK_MIN else t["fg"])
        self._stat(vcf, "VOLT AVG", f"{s['avgVolt']:.2f}V", t["fg"])
        self._stat(vcf, "VOLT MAX", f"{s['maxVolt']:.2f}V",
                   t["red"] if s["maxVolt"] > self.VOLT_OK_MAX else t["fg"])

        # Stat cards - time under/over
        uc = tk.Frame(self, bg=t["card"]); uc.pack(fill="x", padx=20, pady=4)
        ucf = tk.Frame(uc, bg=t["card"]); ucf.pack(fill="x", pady=10)
        self._stat(ucf, "UNDER 12.2V", f"{s['underSecs']}s",
                   t["yellow"] if s["underSecs"] > 0 else t["muted"])
        self._stat(ucf, "OVER 14.8V", f"{s['overSecs']}s",
                   t["red"] if s["overSecs"] > 0 else t["muted"])
        self._stat(ucf, "PRE-RIDE", f"{s['preRideVolt']:.2f}V", t["fg"])

        # Stat cards - temp
        tc = tk.Frame(self, bg=t["card"]); tc.pack(fill="x", padx=20, pady=4)
        tcf = tk.Frame(tc, bg=t["card"]); tcf.pack(fill="x", pady=10)
        self._stat(tcf, "TEMP MIN", f"{s['minTemp']}C", t["fg"])
        self._stat(tcf, "TEMP MAX", f"{s['maxTemp']}C", t["fg"])
        # V4.34: decode flags to a human label and match the graph
        # colour convention (green=OK, yellow=UNDER, red=OVER).
        flags = s.get("flags", 0)
        FLAG_UNDER = 0x01
        FLAG_OVER  = 0x02
        FLAG_WARN  = 0x04 | 0x08 | 0x10 | 0x20   # freezing/hot/storage
        FLAG_PANIC = 0x40
        if flags & FLAG_PANIC:
            flag_str, flag_col = "PANIC", t["red"]
        elif flags & FLAG_OVER:
            flag_str, flag_col = "OVER", t["red"]
        elif flags & FLAG_UNDER:
            flag_str, flag_col = "UNDER", t["yellow"]
        elif flags & FLAG_WARN:
            flag_str, flag_col = "WARN", t["yellow"]
        else:
            flag_str, flag_col = "OK", t["green"]
        self._stat(tcf, "FLAGS", flag_str, flag_col)

        # Voltage graph - auto-scaled, coloured by status
        tk.Label(self, text="VOLTAGE (green=OK, yellow=UNDER, red=OVER)",
                 bg=t["bg"], fg=t["muted"], font=("Helvetica", 10, "bold")).pack(anchor="w", padx=20, pady=(12, 4))
        vcanvas = tk.Canvas(self, width=560, height=140, bg=t["card"], highlightthickness=0)
        vcanvas.pack(padx=20)
        self._draw_voltage_graph(vcanvas, rows)

        # Temp graph
        tk.Label(self, text="TEMPERATURE (C)",
                 bg=t["bg"], fg=t["muted"], font=("Helvetica", 10, "bold")).pack(anchor="w", padx=20, pady=(12, 4))
        tcanvas = tk.Canvas(self, width=560, height=140, bg=t["card"], highlightthickness=0)
        tcanvas.pack(padx=20)
        self._draw_temp_graph(tcanvas, rows)

        # Buttons
        btns = tk.Frame(self, bg=t["bg"]); btns.pack(pady=(16, 20))
        tk.Button(btns, text="Refresh", width=12,
                  font=("Helvetica", 11),
                  command=self._refresh).pack(side="left", padx=6)
        tk.Button(btns, text="Close", width=12,
                  font=("Helvetica", 11),
                  command=self.destroy).pack(side="left", padx=6)

    def _stat(self, parent, label, value, fg):
        t = self.app.theme
        c = tk.Frame(parent, bg=t["card"])
        c.pack(side="left", expand=True, fill="x")
        tk.Label(c, text=label, bg=t["card"], fg=t["muted"],
                 font=("Helvetica", 9)).pack(pady=(2, 0))
        tk.Label(c, text=value, bg=t["card"], fg=fg,
                 font=("Helvetica", 16, "bold")).pack(pady=(0, 2))

    def _draw_voltage_graph(self, canvas, rows):
        t = self.app.theme
        W, H = 560, 140
        pad_l, pad_r, pad_t, pad_b = 46, 10, 10, 24
        pw = W - pad_l - pad_r
        ph = H - pad_t - pad_b

        # Data
        volts = [r["volt"] for r in rows]
        epochs = [r["epoch"] for r in rows]
        if not volts:
            return

        # Auto-scale with padding
        y_min, y_max = min(volts), max(volts)
        if y_max - y_min < 0.5:
            m = (y_max + y_min) / 2
            y_min, y_max = m - 0.5, m + 0.5
        else:
            pad = (y_max - y_min) * 0.15
            y_min -= pad; y_max += pad

        x_min, x_max = min(epochs), max(epochs)
        x_range = (x_max - x_min) or 1
        y_range = (y_max - y_min) or 1

        # Gridlines
        for i in range(4):
            v = y_max - (i / 3) * y_range
            y = pad_t + (i / 3) * ph
            canvas.create_line(pad_l, y, W - pad_r, y, fill=t["grid"])
            canvas.create_text(pad_l - 5, y, text=f"{v:.1f}",
                               fill=t["muted"], font=("Helvetica", 9), anchor="e")

        # Reference lines
        for ref_v, label, colour in [
            (self.VOLT_OK_MIN, "12.2 crank", t["yellow"]),
            (self.VOLT_OK_MAX, "14.8 over",  t["red"]),
        ]:
            if y_min <= ref_v <= y_max:
                y = pad_t + ph - ((ref_v - y_min) / y_range) * ph
                canvas.create_line(pad_l, y, W - pad_r, y, fill=colour,
                                   width=1, dash=(3, 3))
                canvas.create_text(W - pad_r - 4, y - 6, text=label,
                                   fill=colour, font=("Helvetica", 8), anchor="e")

        # Classify each point
        def status(v):
            if v > self.VOLT_OK_MAX: return "over"
            if v < self.VOLT_OK_MIN: return "under"
            return "ok"

        colors = {"ok": t["green"], "under": t["yellow"], "over": t["red"]}

        # Build segments
        points = []
        for e, v in zip(epochs, volts):
            x = pad_l + ((e - x_min) / x_range) * pw
            y = pad_t + ph - ((v - y_min) / y_range) * ph
            points.append((x, y, status(v)))

        runs = []
        current_status = None
        current_points = []
        for x, y, st in points:
            if current_status is None or st != current_status:
                if current_points:
                    runs.append((current_status, current_points))
                current_status = st
                current_points = [(x, y)]
            else:
                current_points.append((x, y))
        if current_points:
            runs.append((current_status, current_points))

        # Draw each run
        for st, pts in runs:
            if len(pts) >= 2:
                flat = []
                for x, y in pts:
                    flat.extend([x, y])
                canvas.create_line(*flat, fill=colors[st], width=2,
                                   capstyle=tk.ROUND, joinstyle=tk.ROUND)
            else:
                x, y = pts[0]
                canvas.create_oval(x-2, y-2, x+2, y+2, fill=colors[st], outline="")

        # X-axis labels
        from datetime import datetime
        for frac in (0.0, 0.5, 1.0):
            t_epoch = x_min + (x_max - x_min) * frac
            x = pad_l + frac * pw
            anchor = "sw" if frac == 0.0 else ("s" if frac == 0.5 else "se")
            canvas.create_text(x, H - 4,
                               text=datetime.fromtimestamp(t_epoch).strftime("%H:%M"),
                               fill=t["muted"], font=("Helvetica", 8), anchor=anchor)

    def _draw_temp_graph(self, canvas, rows):
        t = self.app.theme
        W, H = 560, 140
        pad_l, pad_r, pad_t, pad_b = 46, 10, 10, 24
        pw = W - pad_l - pad_r
        ph = H - pad_t - pad_b

        temps = [r["temp"] for r in rows]
        epochs = [r["epoch"] for r in rows]
        if not temps:
            return

        y_min, y_max = min(temps), max(temps)
        if y_max - y_min < 2:
            m = (y_max + y_min) / 2
            y_min, y_max = m - 1, m + 1
        else:
            pad = (y_max - y_min) * 0.15
            y_min -= pad; y_max += pad

        x_min, x_max = min(epochs), max(epochs)
        x_range = (x_max - x_min) or 1
        y_range = (y_max - y_min) or 1

        for i in range(4):
            v = y_max - (i / 3) * y_range
            y = pad_t + (i / 3) * ph
            canvas.create_line(pad_l, y, W - pad_r, y, fill=t["grid"])
            canvas.create_text(pad_l - 5, y, text=f"{v:.0f}",
                               fill=t["muted"], font=("Helvetica", 9), anchor="e")

        coords = []
        for e, v in zip(epochs, temps):
            x = pad_l + ((e - x_min) / x_range) * pw
            y = pad_t + ph - ((v - y_min) / y_range) * ph
            coords.extend([x, y])
        if len(coords) >= 4:
            canvas.create_line(*coords, fill=t["orange"], width=2,
                               capstyle=tk.ROUND, joinstyle=tk.ROUND)

        from datetime import datetime
        for frac in (0.0, 0.5, 1.0):
            t_epoch = x_min + (x_max - x_min) * frac
            x = pad_l + frac * pw
            anchor = "sw" if frac == 0.0 else ("s" if frac == 0.5 else "se")
            canvas.create_text(x, H - 4,
                               text=datetime.fromtimestamp(t_epoch).strftime("%H:%M"),
                               fill=t["muted"], font=("Helvetica", 8), anchor=anchor)

    def _refresh(self):
        for w in self.winfo_children():
            w.destroy()
        self._build_ui()

if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
