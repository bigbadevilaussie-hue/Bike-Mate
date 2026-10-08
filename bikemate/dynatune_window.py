# === Bike-Mate GUI: dynatune_window ===
# Post-ride diagnostics window. Reads the output of dynatune.analyse()
# and renders it as a KPI board. Stock Tk, no third-party widgets.

import tkinter as tk
from .config import *
from . import state


# Status colours, per theme. Day = lighter card backgrounds, night = darker.
STATUS_COLORS = {
    "day": {
        "PASS": ("#d1fae5", "#065f46"),
        "WARN": ("#fef3c7", "#92400e"),
        "FAIL": ("#fee2e2", "#991b1b"),
        "IDLE": ("#e5e7eb", "#6b7280"),
    },
    "night": {
        "PASS": ("#14532d", "#4ade80"),
        "WARN": ("#713f12", "#fbbf24"),
        "FAIL": ("#7f1d1d", "#f87171"),
        "IDLE": ("#1f2937", "#9ca3af"),
    },
}

STATUS_GLYPH = {"PASS": "✓", "WARN": "!", "FAIL": "✕", "IDLE": "–"}


class DynaTuneWindow(tk.Toplevel):
    """Post-ride diagnostic board."""

    # Section label -> analysis dict key
    SECTIONS = [
        ("CORE",     "ride"),
        ("DATA",     "data"),
        ("DEVICE",   "device"),
        ("HARDWARE", "hardware"),
        ("CONFIG",   "config"),
    ]

    def __init__(self, parent, app, analysis, meta=None):
        super().__init__(parent)
        self.app = app
        self.analysis = analysis or {}
        self.meta = meta or {}
        self.theme = app.theme
        self.is_day = app.is_day

        self.title("Dyna Tune — Bike-Mate")
        self.resizable(True, True)
        self.transient(parent)
        self.minsize(720, 560)
        self.geometry("820x640")

        self.configure(bg=self.theme["bg"])
        self._build()

    def _palette(self):
        key = "day" if self.is_day else "night"
        return STATUS_COLORS[key], self.theme

    def _build(self):
        status_colours, t = self._palette()

        # ── Header ──────────────────────────────────────────────
        header = tk.Frame(self, bg=t["bg"])
        header.pack(fill="x", padx=20, pady=(16, 4))

        tk.Label(header, text="Dyna Tune",
                 bg=t["bg"], fg=t["fg"],
                 font=("Helvetica", 24, "bold")).pack(side="left")

        right = tk.Frame(header, bg=t["bg"])
        right.pack(side="right")
        meta_lines = [
            f"FW {self.meta.get('fw', '—')}",
            self.meta.get("time_range", "—"),
            f"{self.meta.get('hours', '—')} h  ·  {self.meta.get('samples', '—')} samples",
        ]
        settings_note = self.meta.get("settings_note")
        if settings_note:
            meta_lines.append(settings_note)
        for line in meta_lines:
            tk.Label(right, text=line, bg=t["bg"], fg=t["muted"],
                     font=("Helvetica", 10)).pack(anchor="e")

        # ── Overall status block ────────────────────────────────
        block = tk.Frame(self, bg=t["card"], highlightthickness=0)
        block.pack(fill="x", padx=20, pady=(8, 10))

        overall = self._overall_status()
        headline_colour = status_colours.get(overall["level"], status_colours["IDLE"])[1]

        tk.Label(block, text=f"{overall['glyph']}  {overall['title']}",
                 bg=t["card"], fg=headline_colour,
                 font=("Helvetica", 18, "bold")).pack(pady=(14, 2))

        tk.Label(block, text=overall["summary"],
                 bg=t["card"], fg=t["fg"],
                 font=("Helvetica", 11)).pack()

        if overall.get("extra"):
            tk.Label(block, text=overall["extra"],
                     bg=t["card"], fg=t["muted"],
                     font=("Helvetica", 10)).pack(pady=(2, 14))
        else:
            tk.Frame(block, bg=t["card"], height=14).pack()

        # ── Scrollable body ─────────────────────────────────────
        body_wrap = tk.Frame(self, bg=t["bg"])
        body_wrap.pack(fill="both", expand=True, padx=20, pady=(0, 6))

        canvas = tk.Canvas(body_wrap, bg=t["bg"], highlightthickness=0)
        scrollbar = tk.Scrollbar(body_wrap, orient="vertical",
                                 command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        inner = tk.Frame(canvas, bg=t["bg"])
        canvas.create_window((0, 0), window=inner, anchor="nw", tags="inner")

        def _on_resize(_event=None):
            canvas.configure(scrollregion=canvas.bbox("all"))
            canvas.itemconfigure("inner", width=canvas.winfo_width())

        inner.bind("<Configure>", _on_resize)
        canvas.bind("<Configure>", _on_resize)

        # Scroll on mouse wheel
        def _on_wheel(event):
            canvas.yview_scroll(int(-event.delta / 120), "units")
        canvas.bind_all("<MouseWheel>", _on_wheel)

        for label, key in self.SECTIONS:
            self._build_section(inner, label, key, status_colours, t)

        # ── Footer ──────────────────────────────────────────────
        footer = tk.Frame(self, bg=t["card"])
        footer.pack(fill="x", side="bottom")

        footer_bits = []
        for field in ("samples", "hours", "rest_rate", "pre_ride_v", "avg_running_v"):
            val = self.meta.get(field)
            if val is None:
                continue
            unit = ""
            if field == "hours":
                unit = "h"
            elif field in ("pre_ride_v", "avg_running_v"):
                unit = " V"
            footer_bits.append(f"{field.replace('_', ' ')} {val}{unit}")
        footer_text = "   ·   ".join(footer_bits) if footer_bits else ""

        tk.Label(footer, text=footer_text, bg=t["card"], fg=t["muted"],
                 font=("Helvetica", 10)).pack(side="left", padx=16, pady=10)

        tk.Button(footer, text="Close", width=10,
                  bg=t["card"], fg=t["fg"],
                  activebackground=t["accent"], activeforeground=t["fg"],
                  font=("Helvetica", 11),
                  command=self.destroy).pack(side="right", padx=16, pady=8)

    def _build_section(self, parent, label, key, status_colours, t):
        section = self.analysis.get(key, {}) if isinstance(self.analysis, dict) else {}

        frame = tk.Frame(parent, bg=t["card"])
        frame.pack(fill="x", pady=4)

        tk.Label(frame, text=label, bg=t["card"], fg=t["muted"],
                 font=("Helvetica", 11, "bold")).pack(anchor="w",
                                                      padx=14, pady=(10, 4))

        pills = tk.Frame(frame, bg=t["card"])
        pills.pack(fill="x", padx=10, pady=(0, 12))

        if not section:
            tk.Label(pills, text="no data", bg=t["card"], fg=t["muted"],
                     font=("Helvetica", 10)).pack(side="left", padx=8)
            return

        for check_key, result in section.items():
            if not isinstance(result, dict):
                continue
            status = (result.get("status") or "IDLE").upper()
            if status not in ("PASS", "WARN", "FAIL", "IDLE"):
                status = "IDLE"
            bg, fg = status_colours[status]
            glyph = STATUS_GLYPH[status]
            text = f"{glyph}  {check_key.replace('_', ' ').title()}"

            pill = tk.Button(pills, text=text,
                             bg=bg, fg=fg,
                             activebackground=bg, activeforeground=fg,
                             relief="flat", bd=0, padx=10, pady=4,
                             font=("Helvetica", 10),
                             command=lambda k=check_key, r=result: self._show_detail(k, r))
            pill.pack(side="left", padx=3, pady=3)

    def _overall_status(self):
        statuses = []
        for section in self.analysis.values():
            if isinstance(section, dict):
                for check in section.values():
                    if isinstance(check, dict):
                        statuses.append((check.get("status") or "IDLE").upper())

        def count(s):
            return statuses.count(s)

        if count("FAIL") > 0:
            return {
                "level": "FAIL",
                "glyph": "✕",
                "title": "Attention Required",
                "summary": f"{count('FAIL')} FAIL  ·  {count('WARN')} WARN  ·  {count('PASS')} PASS",
                "extra": self.meta.get("runway_note", ""),
            }
        if count("WARN") > 0:
            return {
                "level": "WARN",
                "glyph": "!",
                "title": "Mostly Healthy",
                "summary": f"{count('WARN')} WARN  ·  {count('PASS')} PASS  ·  {count('IDLE')} IDLE",
                "extra": self.meta.get("runway_note", ""),
            }
        return {
            "level": "PASS",
            "glyph": "✓",
            "title": "Battery System Healthy",
            "summary": f"{count('PASS')} PASS  ·  {count('IDLE')} IDLE",
            "extra": self.meta.get("runway_note", ""),
        }

    def _show_detail(self, key, result):
        t = self.theme
        win = tk.Toplevel(self)
        win.title(key.replace("_", " ").title())
        win.configure(bg=t["bg"])
        win.geometry("420x260")
        win.transient(self)
        win.resizable(False, False)

        status = (result.get("status") or "IDLE").upper()
        metric = result.get("metric", "—")
        detail = result.get("detail", "No extra detail")

        status_colours, _ = self._palette()
        _, fg = status_colours.get(status, status_colours["IDLE"])

        tk.Label(win, text=key.replace("_", " ").title(),
                 bg=t["bg"], fg=t["fg"],
                 font=("Helvetica", 14, "bold")).pack(pady=(16, 4))

        tk.Label(win, text=f"Status: {status}",
                 bg=t["bg"], fg=fg,
                 font=("Helvetica", 12, "bold")).pack()

        tk.Label(win, text=f"Metric: {metric}",
                 bg=t["bg"], fg=t["fg"],
                 font=("Helvetica", 11)).pack(pady=(4, 0))

        tk.Label(win, text=detail, wraplength=380, justify="left",
                 bg=t["bg"], fg=t["muted"],
                 font=("Helvetica", 10)).pack(padx=20, pady=(8, 12))

        tk.Button(win, text="Close", width=10, command=win.destroy,
                  font=("Helvetica", 11)).pack(pady=(0, 14))
