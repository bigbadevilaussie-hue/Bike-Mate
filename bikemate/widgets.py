# === Bike-Mate GUI: widgets ===
# auto-extracted, edit here ===

import tkinter as tk
from collections import deque
from .config import *
from . import state


class Graph(tk.Canvas):
    def __init__(self, parent, app, color_key="blue", fill_key=None,
                 y_min=None, y_max=None, w=380, h=120):
        super().__init__(parent, width=w, height=h,
                         bg=app.theme["card"], highlightthickness=0)
        self.app = app
        self.color_key = color_key
        self.fill_key = fill_key or "fill"
        self.y_min, self.y_max = y_min, y_max
        self.w, self.h = w, h
        self.pad_l, self.pad_r, self.pad_t, self.pad_b = 34, 6, 6, 6
        self.data = deque([None] * HIST_LEN, maxlen=HIST_LEN)

    def apply_theme(self):
        t = self.app.theme
        self.configure(bg=t["card"])
        self.redraw()

    def set_data(self, d):
        self.data = d
        self.redraw()

    def redraw(self):
        t = self.app.theme
        self.delete("all")
        self.configure(bg=t["card"])
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
            self.create_line(self.pad_l, y, self.w - self.pad_r, y, fill=t["grid"])
            self.create_text(self.pad_l - 4, y, text=f"{v:.1f}", fill=t["muted"],
                             font=("Helvetica", 9), anchor="e")
        if not pts:
            self.create_text(self.w / 2, self.h / 2, text="waiting…", fill=t["muted"])
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
            self.create_polygon(poly, fill=t[self.fill_key], outline="")
            self.create_line(*coords, fill=t[self.color_key], width=2,
                             capstyle=tk.ROUND, joinstyle=tk.ROUND)
        lx, ly = coords[-2], coords[-1]
        self.create_oval(lx - 3, ly - 3, lx + 3, ly + 3,
                         fill=t[self.color_key], outline="")

