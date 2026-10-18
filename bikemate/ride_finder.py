import tkinter as tk
from datetime import date
from .config import OSA_URL, BG, CARD, FG, MUTED, BLUE
from . import state
from .helpers import *

class RideFinder(tk.Toplevel):
    def __init__(self, parent, app):
        super().__init__(parent)
        self.app = app
        self.title("Ride Finder")
        self.configure(bg=BG)
        self.resizable(True, True)
        self.geometry("700x500")
        self.transient(parent)
        self.rides = []
        self._build()

    def _build(self):
        top = tk.Frame(self, bg=BG)
        top.pack(fill="x", padx=20, pady=(16, 8))
        
        tk.Label(top, text="Select a Ride:", bg=BG, fg=FG, font=("Helvetica", 12, "bold")).pack(side="left")
        
        self.var = tk.StringVar()
        self.opt = tk.OptionMenu(top, self.var, "")
        self.opt.configure(bg=CARD, fg=FG, font=("Helvetica", 11), highlightthickness=0)
        self.opt.pack(side="left", padx=(10, 0))

        btn_row = tk.Frame(self, bg=BG)
        btn_row.pack(fill="x", padx=20, pady=20)
        
        tk.Button(btn_row, text="Show", command=self._on_show, bg=CARD, fg=FG, font=("Helvetica", 11), width=10).pack(side="left")
        tk.Button(btn_row, text="Close", command=self.destroy, bg=CARD, fg=FG, font=("Helvetica", 11), width=10).pack(side="right")

        self.status_lbl = tk.Label(self, text="Loading rides from OSA...", bg=BG, fg=MUTED, font=("Helvetica", 10))
        self.status_lbl.pack(pady=5)

        self.text = tk.Text(self, wrap="none", bg="#f5f6fa", fg="#1e1e2e",
                            font=("Menlo", 11), relief="flat", padx=10, pady=8)
        self.text.pack(fill="both", expand=True, padx=20, pady=(0, 20))

        self._load_rides()

    def _load_rides(self):
        import urllib.request
        import json
        from datetime import datetime
        try:
            url = f"{OSA_URL}/bike/files/list?dir=rides"
            with urllib.request.urlopen(url, timeout=5) as r:
                data = json.loads(r.read().decode())
            
            files = data.get("files", [])
            raw_rides = [f["name"] for f in files if f["name"].startswith("ride_") and f["name"].endswith(".gz")]
            raw_rides.sort(reverse=True)
            
            self.rides = []
            for fname in raw_rides:
                try:
                    # ride_20261019000634.csv.gz -> 20261019000634
                    ts = fname[5:19]
                    dt = datetime.strptime(ts, "%Y%m%d%H%M%S")
                    # Format: Monday 12th Oct 9:00 AM
                    day = dt.strftime("%A")
                    date_num = dt.day
                    suffix = 'th' if 11 <= date_num <= 13 else {1: 'st', 2: 'nd', 3: 'rd'}.get(date_num % 10, 'th')
                    month = dt.strftime("%b")
                    time_str = dt.strftime("%I:%M %p").lstrip("0")
                    
                    label = f"{day} {date_num}{suffix} {month} {time_str}"
                except Exception:
                    label = fname
                self.rides.append((label, fname))

            if not self.rides:
                self.status_lbl.config(text="No rides found on OSA.")
                return

            menu = self.opt["menu"]
            menu.delete(0, "end")
            for label, fname in self.rides:
                menu.add_command(label=label, command=tk._setit(self.var, label))
            
            self.var.set(self.rides[0][0])
            self.status_lbl.config(text=f"Found {len(self.rides)} rides.")

        except Exception as e:
            self.status_lbl.config(text=f"Error: {e}")

    def _on_show(self):
        label = self.var.get()
        if not label:
            return
        ride = next((fname for l, fname in self.rides if l == label), label)
        self.status_lbl.config(text=f"Downloading {ride}...")
        self.text.delete("1.0", "end")
        self.text.insert("1.0", "Loading...")
        self.update_idletasks()
        
        import urllib.request
        import gzip
        
        try:
            url = f"{OSA_URL}/bike/files?dir=rides&name={ride}"
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=10) as r:
                raw_data = r.read()
            
            if ride.endswith(".gz"):
                raw_data = gzip.decompress(raw_data)
                
            text = raw_data.decode("utf-8", errors="ignore")
            lines = text.splitlines()
            
            # Take header + first 50 rows
            preview = "\n".join(lines[:50])
            if len(lines) > 50:
                preview += f"\n\n... ({len(lines) - 50} more lines)"
            
            self.text.delete("1.0", "end")
            self.text.insert("1.0", preview)
            self.status_lbl.config(text=f"Loaded {ride} ({len(lines)-1} points)")
            
        except Exception as e:
            self.text.delete("1.0", "end")
            self.text.insert("1.0", f"Error: {e}")
            self.status_lbl.config(text="Error")
