# === Bike-Mate GUI: serial popup ===
# Day-themed tk window that polls /serial-raw from the bike over HTTP.
# Only works while the bike is in maintenance mode (WiFi up).
# V4.50

import tkinter as tk
from tkinter import messagebox
import threading
import time
import urllib.request

from .config import BIKE_IP


POLL_MS = 2000


class SerialWindow(tk.Toplevel):
    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app
        self.title("Bike-Mate Serial")
        self.geometry("640x420")
        self.minsize(560, 360)
        self.configure(bg="#ffffff")
        self.transient(parent)
        self.focus_force()
        self.running = True
        self.auto_scroll = True
        self.last_text = ""
        self.last_ok = None

        self._build()
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._start_poll()

    # ---- UI ----
    def _build(self):
        # Header row: title left, buttons right
        header = tk.Frame(self, bg="#ffffff", height=44)
        header.pack(fill="x", padx=12, pady=(10, 4))
        header.pack_propagate(False)

        tk.Label(header, text="Bike-Mate Serial",
                 bg="#ffffff", fg="#1e1e2e",
                 font=("Helvetica", 14, "bold")).pack(side="left", padx=(4, 0))

        # Buttons right-aligned, in reverse order (pack side=right)
        self._btn(header, "Close", self._on_close).pack(side="right", padx=(6, 0))
        self._btn(header, "Copy All", self._copy_all).pack(side="right", padx=(6, 0))
        self._btn(header, "Refresh", self._refresh_now).pack(side="right")

        # Text + scrollbar
        body = tk.Frame(self, bg="#ffffff")
        body.pack(fill="both", expand=True, padx=12, pady=(0, 4))

        self.text = tk.Text(body, wrap="none",
                            bg="#f5f6fa", fg="#1e1e2e",
                            font=("Menlo", 11),
                            insertbackground="#1e1e2e",
                            relief="flat", bd=0,
                            highlightthickness=1,
                            highlightbackground="#d8dde8",
                            highlightcolor="#d8dde8",
                            padx=10, pady=8)
        scroll = tk.Scrollbar(body, command=self.text.yview,
                              orient="vertical")
        self.text.configure(yscrollcommand=scroll.set)
        self.text.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

        # Detect user scrolling up -> suspend auto-scroll
        self.text.bind("<MouseWheel>", self._on_wheel)
        self.text.bind("<Button-4>", self._on_wheel)
        self.text.bind("<Button-5>", self._on_wheel)

        # Status line
        self.status = tk.Label(self, text="Connecting…",
                               bg="#ffffff", fg="#7a8194",
                               font=("Helvetica", 10), anchor="w")
        self.status.pack(fill="x", padx=16, pady=(0, 8))

    def _btn(self, parent, label, cmd):
        return tk.Button(parent, text=label, command=cmd,
                         bg="#e6e9f0", fg="#1e1e2e",
                         activebackground="#d8dde8",
                         activeforeground="#1e1e2e",
                         relief="flat", bd=0,
                         padx=14, pady=4,
                         font=("Helvetica", 11),
                         cursor="hand2",
                         highlightthickness=0)

    # ---- polling ----
    def _start_poll(self):
        t = threading.Thread(target=self._poll_loop, daemon=True)
        t.start()

    def _poll_loop(self):
        while self.running:
            self._fetch_once()
            # Sleep in small increments so close is responsive
            for _ in range(POLL_MS // 100):
                if not self.running:
                    return
                time.sleep(0.1)

    def _fetch_once(self):
        url = f"http://{BIKE_IP}/serial-raw"
        try:
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=3) as r:
                body = r.read().decode(errors="replace")
            ok = True
            err = None
        except Exception as e:
            body = ""
            ok = False
            err = str(e)
        # Marshal back to Tk thread
        try:
            self.after(0, lambda: self._apply(body, ok, err))
        except Exception:
            pass

    def _apply(self, body, ok, err):
        if not self.running:
            return
        # Only redraw when content changed
        if ok and body != self.last_text:
            self.last_text = body
            # Preserve scroll position if user has scrolled up
            at_bottom = self._at_bottom()
            self.text.configure(state="normal")
            self.text.delete("1.0", "end")
            self.text.insert("1.0", body)
            self.text.configure(state="disabled")
            if at_bottom or self.auto_scroll:
                self.text.see("end")
        if ok:
            n = body.count("\n")
            self.status.configure(
                text=f"Connected · {n} lines · {time.strftime('%H:%M:%S')}",
                fg="#7a8194")
        else:
            msg = "Not in maintenance mode" if "refused" in (err or "").lower() or "timed out" in (err or "").lower() else f"Unreachable: {err}"
            self.status.configure(text=msg, fg="#d20f39")
        self.last_ok = ok

    def _at_bottom(self):
        try:
            y0, y1 = self.text.yview()
            return y1 >= 0.99
        except Exception:
            return True

    def _on_wheel(self, event):
        # Called when the user scrolls. If they scroll up, suspend
        # auto-scroll. If they reach the bottom again, resume.
        self.auto_scroll = False
        self.after(200, self._maybe_resume)

    def _maybe_resume(self):
        if self._at_bottom():
            self.auto_scroll = True

    # ---- buttons ----
    def _refresh_now(self):
        threading.Thread(target=self._fetch_once, daemon=True).start()

    def _copy_all(self):
        try:
            self.clipboard_clear()
            self.clipboard_append(self.last_text)
            self.status.configure(text="Copied to clipboard", fg="#7a8194")
        except Exception as e:
            messagebox.showerror("Copy failed", str(e))

    def _on_close(self):
        self.running = False
        self.destroy()
