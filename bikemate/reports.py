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




if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
