# === Bike-Mate GUI: app ===
# auto-extracted, edit here ===

import tkinter as tk
import tkinter.ttk
from tkinter import messagebox
import threading, time, os, sys, json, asyncio, shutil, requests, webbrowser
from datetime import datetime
from .config import *
from . import state
from .helpers import *
from .widgets import *
from .weather import *
from .ota import *
from .drive import *
from .ble import *
from .reports import *


class App:
    def __init__(self, root):
        self.root = root
        self.theme = current_theme()
        self.is_day = is_daytime()
        root.title("Bike-Mate")
        root.configure(bg=self.theme["bg"])
        root.resizable(False, False)
        start_ota_server()
        start_drive_server()
        self.worker = BLEWorker()
        mb = tk.Menu(root)
        am = tk.Menu(mb, tearoff=0)
        drive_menu = tk.Menu(am, tearoff=0)
        drive_menu.add_command(label="☁️  Open Cloud Drive",
                               command=self.open_drive)
        drive_menu.add_command(label="💾  Open Local Drive",
                               command=self.open_local_drive)
        drive_menu.add_separator()
        drive_menu.add_command(label="⬇️  Sync from Drive",
                               command=self.menu_sync)
        am.add_cascade(label="☁️ Drive", menu=drive_menu)
        am.add_separator()
        # V4.26: maint actions consolidated under one submenu.
        maint_menu = tk.Menu(am, tearoff=0)
        maint_menu.add_command(label="🔧  Activate",
                               command=self.activate_maintenance)
        maint_menu.add_command(label="🛑  Deactivate",
                               command=self.deactivate_maintenance)
        maint_menu.add_separator()
        maint_menu.add_command(label="📤  Update Firmware",
                               command=self.menu_ota)
        maint_menu.add_command(label="⚙️  Settings",
                               command=self.open_settings)
        maint_menu.add_separator()
        maint_menu.add_command(label="📟  Open Serial Page",
                               command=self._open_serial_page)
        am.add_cascade(label="🔧 Maintenance", menu=maint_menu)
        self.maint_menu = maint_menu
        am.add_separator()
        reports_menu = tk.Menu(am, tearoff=0)
        reports_menu.add_command(label="Last Ride",    command=self.open_last_ride_report)
        reports_menu.add_separator()
        reports_menu.add_command(label="Last 2 Hours", command=lambda: self.open_report("2h"))
        reports_menu.add_command(label="Daily",        command=lambda: self.open_report("day"))
        reports_menu.add_command(label="Weekly",       command=lambda: self.open_report("week"))
        am.add_cascade(label="📊 Reports", menu=reports_menu)
        am.add_separator()
        am.add_command(label="🌗 Toggle Day/Night", command=self.toggle_theme)
        am.add_separator()
        am.add_command(label="Quit", command=self.on_quit)
        mb.add_cascade(label="🏍️ Bike-Mate", menu=am)
        root.config(menu=mb)
        tk.Label(root, text="🏍️ Bike-Mate", bg=BG, fg=BLUE,
                 font=("Helvetica", 26, "bold")).pack(pady=(20, 4))
        self.weather_label = tk.Label(root, text="Atkinsons Dam · --.-° ⏳",
                                      bg=BG, fg=FG,
                                      font=("Helvetica", 20, "bold"))
        self.weather_label.pack(pady=(0, 12))
        bc = tk.Frame(root, bg=CARD); bc.pack(fill="x", padx=20, pady=6)
        br = tk.Frame(bc, bg=CARD); br.pack(fill="x", pady=10)
        self.acc_lbl = self._col(br, "ACC", "--")
        self.eng_lbl = self._col(br, "Engine", "--")
        self.warn_lbl = self._col(br, "Warn", "--")
        sf = tk.Frame(root, bg=CARD); sf.pack(fill="x", padx=20, pady=6)
        self.state_lbl = tk.Label(sf, text="💤 MONITOR", bg=CARD, fg=BLUE,
                                  font=("Helvetica", 26, "bold"))
        self.state_lbl.pack(pady=14)
        self.maint_lbl = tk.Label(sf, text="", bg=CARD, fg=YELLOW,
                                  font=("Helvetica", 13, "bold"),
                                  padx=12, pady=4)
        self.maint_lbl.pack(pady=(0, 8))
        vc = tk.Frame(root, bg=CARD); vc.pack(fill="x", padx=20, pady=6)
        self.volt_lbl = tk.Label(vc, text="--.-- V", bg=CARD, fg=GREEN,
                                 font=("Helvetica", 46, "bold"))
        self.volt_lbl.pack(pady=14)
        rc = tk.Frame(root, bg=CARD); rc.pack(fill="x", padx=20, pady=6)
        ri = tk.Frame(rc, bg=CARD); ri.pack(fill="x", pady=6)
        self.temp_lbl = self._col(ri, "🏍️ Temp", "--.-C")
        self.time_lbl = self._col(ri, "Time", "--:--")
        lr = tk.Frame(root, bg=CARD); lr.pack(fill="x", padx=20, pady=6)
        tk.Label(lr, text="Last Ride", bg=CARD, fg=MUTED,
                 font=("Helvetica", 10)).pack(pady=(8, 2))
        self.last_ride_lbl = tk.Label(lr, text="none yet", bg=CARD, fg=MUTED,
                                      font=("Helvetica", 13, "bold"))
        self.last_ride_lbl.pack()
        self.last_ride_sub = tk.Label(lr, text="", bg=CARD, fg=MUTED,
                                      font=("Helvetica", 10))
        self.last_ride_sub.pack(pady=(2, 0))
        self.last_ride_pre = tk.Label(lr, text="", bg=CARD, fg=MUTED,
                                      font=("Helvetica", 11, "bold"))
        self.last_ride_pre.pack(pady=(0, 8))
        vgc = tk.Frame(root, bg=CARD); vgc.pack(fill="x", padx=20, pady=6)
        self.volt_graph = Graph(vgc, self, color_key="green", y_min=11.5, y_max=15.0)
        self.volt_graph.pack(padx=8, pady=8)
        tgc = tk.Frame(root, bg=CARD); tgc.pack(fill="x", padx=20, pady=6)
        self.temp_graph = Graph(tgc, self, color_key="orange", y_min=15.0, y_max=40.0)
        self.temp_graph.pack(padx=8, pady=8)
        self.footer_lbl = tk.Label(root, text=f"GUI v{GUI_VERSION}",
                                   bg=BG, fg=MUTED,
                                   font=("Helvetica", 9))
        self.footer_lbl.pack(side="bottom", pady=6)
        root.protocol("WM_DELETE_WINDOW", self.on_quit)
        threading.Thread(target=weather_thread_loop, daemon=True).start()
        self.worker.start()
        self.apply_theme()
        self.theme_check()
        self._check_maint_on_startup()
        self.tick()

    def _col(self, parent, label, value):
        c = tk.Frame(parent, bg=CARD); c.pack(side="left", expand=True, fill="x")
        tk.Label(c, text=label, bg=CARD, fg=MUTED,
                 font=("Helvetica", 11)).pack(pady=(4, 2))
        v = tk.Label(c, text=value, bg=CARD, fg=FG,
                     font=("Helvetica", 16, "bold"))
        v.pack(pady=(0, 4))
        return v

    def toggle_theme(self):
        self.is_day = not self.is_day
        self.theme = THEME_DAY if self.is_day else THEME_NIGHT
        print(f"[THEME] toggled to {'day' if self.is_day else 'night'}")
        self.apply_theme()

    def theme_check(self):
        if is_daytime() != self.is_day:
            self.is_day = is_daytime()
            self.theme = current_theme()
            print(f"[THEME] auto-switched to {'day' if self.is_day else 'night'}")
            self.apply_theme()
        self.root.after(60_000, self.theme_check)

    def apply_theme(self):
        t = self.theme
        # V4.34: legacy colour aliases (BG, FG, GREEN, ...) are
        # snapshotted into every module via `from .config import *`.
        # Rebinding them in config.py alone is not enough - we have to
        # re-set each name in each module's namespace or widgets keep
        # drawing with the night palette after the theme flips.
        _alias = {
            "BG": t["bg"], "CARD": t["card"], "GRID": t["grid"],
            "FG": t["fg"], "BLUE": t["blue"], "GREEN": t["green"],
            "YELLOW": t["yellow"], "ORANGE": t["orange"],
            "MUTED": t["muted"], "RED": t["red"],
            "FILL_GREEN": t["fill"], "FILL_BLUE": t["fill"],
        }
        for _modname in ("bikemate.app", "bikemate.widgets", "bikemate.reports",
                         "bikemate.helpers", "bikemate.ble", "bikemate.ota",
                         "bikemate.drive", "bikemate.weather"):
            _mod = sys.modules.get(_modname)
            if _mod is None:
                continue
            for _k, _v in _alias.items():
                if hasattr(_mod, _k):
                    setattr(_mod, _k, _v)
        self.root.configure(bg=t["bg"])

        # Widgets whose bg/fg are driven by special logic, not just theme
        special_fg = {
            self.acc_lbl:    ("green" if state.latest_data.get("a") else "muted"),
            self.eng_lbl:    ("green" if state.latest_data.get("e") else "muted"),
            self.warn_lbl:   ("red"   if state.latest_data.get("w") else "muted"),
            self.volt_lbl:   None,  # set by tick()
            self.state_lbl:  None,  # set by tick()
            self.temp_lbl:   None,  # set by tick()
            self.last_ride_lbl:  None,
            self.last_ride_sub:  None,
            self.last_ride_pre:  None,
        }

        def walk(w):
            try:
                cls = w.winfo_class()
            except Exception:
                return
            try:
                if cls == "Frame":
                    w.configure(bg=t["card"] if w is not self.root else t["bg"])
                elif cls == "Label":
                    # Which frame is this label in? card or bg?
                    parent_bg = t["bg"]
                    try:
                        if w.master.winfo_class() == "Frame" and w.master is not self.root:
                            parent_bg = t["card"]
                    except Exception:
                        pass
                    w.configure(bg=parent_bg)
                    # Don't override fg for special labels
                    if w in special_fg:
                        key = special_fg[w]
                        if key is not None:
                            w.configure(fg=t[key])
                    else:
                        # heuristic: muted for the small column headers,
                        # fg for values. Check font size.
                        try:
                            font = w.cget("font")
                            fs = int(str(font).split()[-1].rstrip(")")) if "bold" not in str(font) else 0
                        except Exception:
                            fs = 0
                        w.configure(fg=t["fg"] if fs == 0 or fs >= 12 else t["muted"])
                elif cls == "Button":
                    w.configure(bg=t["card"], fg=t["fg"],
                                activebackground=t["accent"],
                                activeforeground=t["fg"])
                elif cls == "Entry":
                    w.configure(bg=t["card"], fg=t["fg"],
                                insertbackground=t["fg"])
                elif cls == "Menu":
                    pass  # menu colours are macOS-native
            except Exception:
                pass
            for c in w.winfo_children():
                walk(c)

        walk(self.root)

        # Graphs need explicit apply_theme
        try:
            self.volt_graph.apply_theme()
        except Exception:
            pass
        try:
            self.temp_graph.apply_theme()
        except Exception:
            pass

    def open_report(self, mode):
        BikeReport(self.root, self, mode)

    def open_last_ride_report(self):
        with state.latest_ride_lock:
            r = state.latest_ride
        if r and r.get("summary"):
            LastRideReport(self.root, self)
        else:
            WaitingForRide(self.root, self)

    def open_last_ride_report(self):
        # If we have a cached ride in memory, show it.
        with state.latest_ride_lock:
            r = state.latest_ride
        if r and r.get("summary"):
            LastRideReport(self.root, self)
            return
        # Otherwise open a waiting window that polls for up to 3 wake cycles.
        WaitingForRide(self.root, self)

    def on_quit(self):
        stop_ota_server()
        stop_drive_server()
        self.root.destroy()

    def open_drive(self):
        os.system(f'open "{DRIVE_FOLDER_URL}"')

    def open_local_drive(self):
        os.makedirs(DRIVE_DIR, exist_ok=True)
        os.system(f'open "{DRIVE_DIR}"')

    def open_reports(self):
        from datetime import date, timedelta
        y = (date.today() - timedelta(days=1)).strftime("%Y-%m-%d")
        menu = tk.Menu(self.root, tearoff=0)
        menu.add_command(label=f"Last Day ({y})",
                         command=lambda: BikeReport(self.root, self, "day"))
        menu.add_command(label="Last Ride",
                         command=lambda: BikeReport(self.root, self, "ride"))
        menu.add_command(label="Last Week",
                         command=lambda: BikeReport(self.root, self, "week"))
        menu.tk_popup(self.root.winfo_pointerx(), self.root.winfo_pointery())

    def menu_sync(self):
        def _worker():
            set_status("syncing...")
            dl, total = sync_from_drive()
            set_status("")
            if dl < 0 and total < 0:
                return
            if dl == 0:
                self.root.after(0, lambda: messagebox.showinfo(
                    "Sync", "No new files"))
            elif dl > 0:
                self.root.after(0, lambda: messagebox.showinfo(
                    "Sync", f"Sync complete: {dl} files"))
            else:
                self.root.after(0, lambda: messagebox.showerror(
                    "Sync", "Sync failed"))
        threading.Thread(target=_worker, daemon=True).start()

    def _auto_close_dialog(self, title, message, seconds=2):
        dlg = tk.Toplevel(self.root)
        dlg.title(title)
        dlg.configure(bg=self.theme["bg"])
        dlg.resizable(False, False)
        dlg.transient(self.root)
        tk.Label(dlg, text=message, bg=self.theme["bg"], fg=self.theme["fg"],
                 font=("Helvetica", 11), padx=20, pady=20,
                 justify="left").pack()
        dlg.update_idletasks()
        w = dlg.winfo_width()
        h = dlg.winfo_height()
        x = self.root.winfo_rootx() + (self.root.winfo_width() - w) // 2
        y = self.root.winfo_rooty() + (self.root.winfo_height() - h) // 2
        dlg.geometry(f"+{x}+{y}")
        dlg.after(seconds * 1000, dlg.destroy)

    def menu_ota(self):
        # V4.32: match Fan-Mate's OTA flow. Info dialog with MD5, then
        # a background thread that archives the local .bin and POSTs
        # it. No live progress window — the bike's single-threaded
        # HTTP server can't serve /ota-progress during the upload, so
        # a progress bar would lie. Console output is the record.
        with state.maintenance_lock:
            m = state.maintenance_state
        if m != "ON":
            messagebox.showwarning(
                "OTA",
                "Activate Maintenance Mode first.\n\n"
                "OTA runs over WiFi while the bike is in maintenance.")
            return

        if not os.path.isfile(BUILD_BIN):
            messagebox.showerror(
                "OTA",
                f"Firmware not found:\n{BUILD_BIN}\n\n"
                f"Run Sketch \u2192 Export Compiled Binary in Arduino IDE first.")
            return

        src_ver = read_firmware_version() or "?"

        # During maint, BLE is dead so latest_data["fv"] is stale.
        # Ask the bike directly over HTTP.
        import urllib.request
        try:
            with urllib.request.urlopen(
                    f"http://{BIKE_IP}/version", timeout=3) as r:
                dev_ver = r.read().decode().strip() or "?"
        except Exception:
            dev_ver = state.latest_data.get("fv", "?")

        size = os.path.getsize(BUILD_BIN)
        size_mb = size / (1024 * 1024)
        md5 = compute_md5(BUILD_BIN) or "?"

        try:
            bin_mtime = os.path.getmtime(BUILD_BIN)
            cfg_mtime = os.path.getmtime(CONFIG_H)
            stale = cfg_mtime > bin_mtime
            built_str = datetime.fromtimestamp(bin_mtime).strftime("%Y-%m-%d %H:%M")
        except Exception:
            stale = False
            built_str = "?"

        msg = (
            f"File:        bike_mate.ino.bin\n"
            f"Size:        {size:,} bytes ({size_mb:.2f} MB)\n"
            f"MD5:         {md5[:16]}...\n"
            f"Source ver:  V{src_ver}\n"
            f"Built:       {built_str}\n"
            f"Device ver:  V{dev_ver}\n"
        )
        if stale:
            msg += ("\n\u26a0\ufe0f  Config.h is newer than the .bin.\n"
                    "Re-export the binary before updating.")
        msg += "\n\nProceed with OTA update?"

        if not messagebox.askyesno("Bike-Mate OTA", msg):
            return

        threading.Thread(target=self._ota_worker,
                         args=(src_ver,),
                         daemon=True).start()

    def _ota_worker(self, src_ver):
        print("=" * 50)
        print(f"[OTA] starting")
        print(f"[OTA] version: {src_ver}")
        print(f"[OTA] size:    {os.path.getsize(BUILD_BIN):,} bytes")

        # Archive the local .bin before flashing. Same pattern as
        # Fan-Mate. Keeps every version that has been pushed to the
        # bike, plus a rolling 'latest'.
        try:
            fw_dir = os.path.join(OTA_ARCHIVE_DIR, "firmware")
            os.makedirs(fw_dir, exist_ok=True)
            ts = datetime.now().strftime("%Y%m%d-%H%M")
            archived = os.path.join(fw_dir, f"bike_mate-v{src_ver}-{ts}.bin")
            shutil.copy2(BUILD_BIN, archived)
            shutil.copy2(BUILD_BIN, os.path.join(fw_dir, "bike_mate-latest.bin"))
            print(f"[OTA] archived: {archived}")
        except Exception as e:
            print(f"[OTA] archive failed: {e}")

        print(f"[OTA] uploading...")
        t0 = time.time()
        try:
            url = f"http://{BIKE_IP}/ota?ver={src_ver}"
            with open(BUILD_BIN, "rb") as f:
                r = requests.post(
                    url,
                    files={"firmware": ("bike_mate.ino.bin", f,
                                        "application/octet-stream")},
                    timeout=180)
            dt = time.time() - t0
            print(f"[OTA] HTTP {r.status_code} ({dt:.1f}s): {r.text[:80]}")

            if r.status_code != 200:
                body = r.text[:200]
                self.root.after(0, lambda b=body: messagebox.showerror(
                    "OTA", f"Failed: HTTP {r.status_code}\n{b}"))
            else:
                with state.maintenance_lock:
                    state.maintenance_state = "OFF"
                print(f"[OTA] uploaded V{src_ver} in {dt:.1f}s, device rebooting")
        except Exception as e:
            err = str(e)
            print(f"[OTA] EXCEPTION: {err}")
            self.root.after(0, lambda m=err: messagebox.showerror(
                "OTA", f"Failed:\n{m}"))

        print("[OTA] done")
        print("=" * 50)

    def _update_maint_menu(self):
        # V4.28: enable/disable maint submenu items based on state.
        # Indices in the maint submenu:
        #   0 = Activate
        #   1 = Deactivate
        #   2 = separator
        #   3 = Update Firmware
        #   4 = Settings
        #   5 = separator
        #   6 = Open Serial Page
        if not hasattr(self, "maint_menu"):
            return
        with state.maintenance_lock:
            m = state.maintenance_state
        on = (m == "ON")
        off = (m == "OFF")
        self.maint_menu.entryconfig(0, state=("normal" if off else "disabled"))
        self.maint_menu.entryconfig(1, state=("normal" if on  else "disabled"))
        st = "normal" if on else "disabled"
        for idx in (3, 4, 6):
            try:
                self.maint_menu.entryconfig(idx, state=st)
            except Exception:
                pass

    def activate_maintenance(self):
        with state.maintenance_lock:
            if state.maintenance_state != "OFF":
                self._auto_close_dialog(
                    "Maintenance",
                    f"Already {state.maintenance_state.lower()}.",
                    seconds=2)
                return
            state.maintenance_state = "PENDING"
            state.maint_pending_since = time.time()
            state.maint_pending_timeout = pending_timeout_seconds()
        print(f"[MAINT] pending, timeout {state.maint_pending_timeout}s")
        self._send_maint_command("on")

    def deactivate_maintenance(self):
        with state.maintenance_lock:
            if state.maintenance_state == "OFF":
                self._auto_close_dialog(
                    "Maintenance",
                    "Already off.",
                    seconds=2)
                return

        # V4.34: off path is HTTP, not BLE. BLE is dead while maint is up.
        # Keep state ON until the off-poll confirms the server has gone
        # away. Do NOT set PENDING here - PENDING is for activate, and
        # flipping to it during deactivate confuses the UI. Only start
        # the off-poll AFTER the POST returns, so a failed POST doesn't
        # leave us polling a bike that never got the command.
        import urllib.request

        def _post():
            ok = False
            try:
                req = urllib.request.Request(
                    f"http://{BIKE_IP}/maint/off",
                    method="POST")
                with urllib.request.urlopen(req, timeout=5) as r:
                    print(f"[MAINT] /maint/off -> HTTP {r.status}")
                    ok = True
            except Exception as e:
                print(f"[MAINT] /maint/off failed: {e}")
            if ok:
                self.root.after(0, self._start_maint_off_poll)

        threading.Thread(target=_post, daemon=True).start()

    def _start_maint_off_poll(self):
        # V4.34: require 2 consecutive unreachable responses before
        # flipping state to OFF. A single dropped request during the
        # firmware's WiFi teardown is normal - the old code bounced
        # OFF on the first miss, then the HTTP probe saw the server
        # still up and flipped it back to ON, causing UI flicker.
        import urllib.request
        started = time.time()
        url = f"http://{BIKE_IP}/serial-raw"
        misses = {"n": 0}

        def poll():
            elapsed = time.time() - started
            if elapsed > 90:
                print("[MAINT] off poll: 90s elapsed, force OFF")
                with state.maintenance_lock:
                    state.maintenance_state = "OFF"
                return
            up = False
            try:
                with urllib.request.urlopen(url, timeout=2) as r:
                    up = (r.status == 200)
            except Exception:
                up = False
            if up:
                misses["n"] = 0
            else:
                misses["n"] += 1
                print(f"[MAINT] off poll: miss {misses['n']}/2")
                if misses["n"] >= 2:
                    print("[MAINT] off poll: server down twice, state -> OFF")
                    with state.maintenance_lock:
                        state.maintenance_state = "OFF"
                    return
            self.root.after(2000, poll)

        self.root.after(2000, poll)

    def _send_maint_command(self, on_off):
        payload = '{"maint":"' + on_off + '"}'
        print(f"[MAINT] send {payload}")

        # Fire-and-forget the BLE write, then poll the bike's IP for
        # the serial server. Firmware sets a flag on BLE receipt and
        # enters maintenance on its NEXT wake, so reachability of the
        # serial page is the real signal that maintenance is running.
        def on_result(ok, detail):
            print(f"[MAINT] BLE result for '{on_off}': {ok} ({detail})")

        self.worker.send_ota_command(payload, on_result)

        if on_off == "on":
            self._start_maint_poll()

    def _start_maint_poll(self):
        import urllib.request
        started = time.time()
        url = f"http://{BIKE_IP}/serial-raw"

        def poll():
            elapsed = time.time() - started
            if elapsed > 11 * 60:
                print("[MAINT] poll: 11 min elapsed, bike never entered")
                with state.maintenance_lock:
                    state.maintenance_state = "OFF"
                return
            try:
                with urllib.request.urlopen(url, timeout=3) as r:
                    if r.status == 200:
                        print(f"[MAINT] poll: bike is live after {int(elapsed)}s")
                        with state.maintenance_lock:
                            state.maintenance_state = "ON"
                        # V4.26: no auto-open serial page prompt.
                        # Use the Maintenance menu to open it on demand.
                        return
            except Exception:
                pass
            self.root.after(5000, poll)

        self.root.after(5000, poll)

    def _check_maint_on_startup(self):
        # On GUI start, ask the bike's IP once whether maint is already
        # running. Covers GUI restarts while the bike is mid-maintenance.
        import urllib.request
        url = f"http://{BIKE_IP}/serial-raw"

        def probe():
            try:
                with urllib.request.urlopen(url, timeout=3) as r:
                    if r.status == 200:
                        print("[MAINT] startup: bike already in maint, state -> ON")
                        with state.maintenance_lock:
                            state.maintenance_state = "ON"
            except Exception:
                pass

        threading.Thread(target=probe, daemon=True).start()

    # V4.26: _ask_open_serial_page removed. Serial page opens from the
    # Maintenance menu only.

    def _open_serial_page(self):
        import webbrowser
        url = f"http://{BIKE_IP}/serial"
        print(f"[MAINT] opening {url}")
        webbrowser.open(url)

    def _maint_ack_timeout(self, requested):
        with state.maintenance_lock:
            if state.maintenance_state != "PENDING":
                print(f"[MAINT] timeout fired but state={state.maintenance_state}, ignoring")
                return
            state.maintenance_state = "OFF"
            print(f"[MAINT] timeout: state -> OFF")
        print(f"[MAINT] no ACK for '{requested}', reverting to OFF")
        self._auto_close_dialog(
            "Maintenance",
            f"No ACK from device for '{requested}'.\n"
            f"Firmware command not yet implemented.",
            seconds=3)

    def open_settings(self):
        # V4.33: during maint, BLE is off. Read settings over HTTP.
        with state.maintenance_lock:
            _maint = state.maintenance_state
        if _maint == "ON":
            def _http_read():
                import urllib.request
                try:
                    with urllib.request.urlopen(
                            f"http://{BIKE_IP}/settings", timeout=5) as r:
                        js = json.loads(r.read().decode())
                    print(f"[SET-DBG] http read: {js}")
                    self.root.after(0, lambda: self._show_settings_dialog(js))
                except Exception as exc:
                    print(f"[SET-DBG] http read failed: {exc}")
                    msg = f"Read failed:\n{exc}"
                    self.root.after(0, lambda m=msg: messagebox.showerror("Settings", m))
            threading.Thread(target=_http_read, daemon=True).start()
            return
        def _read():
            print("[SET-DBG] open_settings triggered")
            t0 = time.time()
            while time.time() - t0 < 60:
                if self.worker.client and self.worker.client.is_connected:
                    break
                time.sleep(0.5)
            print(f"[SET-DBG] client connected: {bool(self.worker.client and self.worker.client.is_connected)}")
            if not (self.worker.client and self.worker.client.is_connected):
                self.root.after(0, lambda: messagebox.showerror(
                    "Settings", "Device didn't wake within 60s. Try again."))
                return

            async def _do():
                print(f"[SET-DBG] running _do, uuid={SETTINGS_UUID}")
                c = self.worker.client
                print(f"[SET-DBG] is_connected: {c.is_connected}")
                print(f"[SET-DBG] services attr: {hasattr(c, 'services')}")
                try:
                    svcs = list(c.services)
                    print(f"[SET-DBG] service count: {len(svcs)}")
                    for svc in svcs:
                        print(f"[SET-DBG]   service: {svc.uuid}")
                        for ch in svc.characteristics:
                            print(f"[SET-DBG]     char: {ch.uuid} props={ch.properties}")
                except Exception as ex:
                    print(f"[SET-DBG] services enumeration failed: {ex}")

                got = asyncio.Event()
                buf = {"data": None}
                def on_notify(sender, data):
                    print(f"[SET-DBG] notify received, {len(bytes(data))} bytes")
                    buf["data"] = bytes(data)
                    got.set()

                try:
                    print(f"[SET-DBG] start_notify on {SETTINGS_UUID}")
                    await c.start_notify(SETTINGS_UUID, on_notify)
                    print("[SET-DBG] start_notify OK")
                except Exception as ex:
                    print(f"[SET-DBG] start_notify FAILED: {ex}")
                    raise

                await asyncio.sleep(0.3)
                try:
                    print("[SET-DBG] writing 0x01")
                    await c.write_gatt_char(SETTINGS_UUID, bytes([0x01]))
                    print("[SET-DBG] write OK")
                except Exception as ex:
                    print(f"[SET-DBG] write FAILED: {ex}")
                    raise

                try:
                    await asyncio.wait_for(got.wait(), timeout=8)
                    print(f"[SET-DBG] got notify: {buf['data'][:80]}")
                except asyncio.TimeoutError:
                    print("[SET-DBG] notify TIMEOUT after 8s")

                try:
                    await c.stop_notify(SETTINGS_UUID)
                except Exception:
                    pass
                return buf["data"]

            try:
                fut = asyncio.run_coroutine_threadsafe(_do(), self.worker.loop)
                raw = fut.result(timeout=12)
                print(f"[SET-DBG] raw result: {raw}")
                if raw is None:
                    self.root.after(0, lambda: messagebox.showerror(
                        "Settings", "No data returned (timeout)"))
                    return
                js = json.loads(raw.decode())
                print(f"[SET-DBG] parsed: {js}")
                self.root.after(0, lambda: self._show_settings_dialog(js))
            except Exception as exc:
                print(f"[SET-DBG] outer exception: {exc}")
                msg = f"Read failed:\n{exc}"
                self.root.after(0, lambda m=msg: messagebox.showerror("Settings", m))
        threading.Thread(target=_read, daemon=True).start()

    def _show_settings_dialog(self, current):
        dlg = tk.Toplevel(self.root)
        dlg.title("Bike-Mate Settings")
        dlg.configure(bg=BG)
        dlg.resizable(False, False)
        fields = {}
        def section(title):
            tk.Label(dlg, text=title, bg=BG, fg=BLUE,
                     font=("Helvetica", 12, "bold")).pack(pady=(16, 6), padx=20, anchor="w")
        def add_row(label, key, value, unit):
            row = tk.Frame(dlg, bg=BG); row.pack(fill="x", padx=20, pady=4)
            tk.Label(row, text=label, bg=BG, fg=FG, width=20, anchor="w",
                     font=("Helvetica", 11)).pack(side="left")
            e = tk.Entry(row, width=10, font=("Helvetica", 11))
            e.insert(0, f"{value:.2f}"); e.pack(side="left")
            tk.Label(row, text=unit, bg=BG, fg=MUTED, width=4, anchor="w",
                     font=("Helvetica", 11)).pack(side="left")
            fields[key] = e
        # V5.03: firmware sends compact keys now: {"r":{...},"m":{...}}
        r = current.get("r", current.get("run", {}))
        m = current.get("m", current.get("monitor", {}))
        # map compact keys to internal names
        r = {
            "running_enter": r.get("on",  r.get("running_enter", 13.8)),
            "running_exit":  r.get("off", r.get("running_exit",  13.0)),
            "run_under":     r.get("un",  r.get("run_under",     13.0)),
            "run_over":      r.get("ov",  r.get("run_over",      14.8)),
        }
        m = {
            "normal":  m.get("nrm", m.get("normal",  12.5)),
            "warning": m.get("wrn", m.get("warning", 12.4)),
            "panic":   m.get("pan", m.get("panic",   12.2)),
        }

        section("🚀 RUN MODE")
        add_row("Engine start",  "running_enter", r.get("running_enter", 13.8), "V")
        add_row("Engine stop",   "running_exit",  r.get("running_exit",  13.0), "V")
        add_row("Under (charge)", "run_under",    r.get("run_under",     13.0), "V")
        add_row("Over (reg)",    "run_over",      r.get("run_over",      14.8), "V")

        section("🛌 MONITOR MODE")
        add_row("Normal floor",  "normal",  m.get("normal",  12.5), "V")
        add_row("Warning",       "warning", m.get("warning", 12.4), "V")
        add_row("Panic",         "panic",   m.get("panic",   12.2), "V")

        btn_row = tk.Frame(dlg, bg=BG); btn_row.pack(pady=20)
        def apply():
            try:
                payload = {
                    "r": {
                        "on":  float(fields["running_enter"].get()),
                        "off": float(fields["running_exit"].get()),
                        "un":  float(fields["run_under"].get()),
                        "ov":  float(fields["run_over"].get()),
                    },
                    "m": {
                        "nrm": float(fields["normal"].get()),
                        "wrn": float(fields["warning"].get()),
                        "pan": float(fields["panic"].get()),
                    },
                }
            except ValueError:
                messagebox.showerror("Settings", "Invalid number"); return
            js = json.dumps(payload, separators=(",", ":"))
            def _write():
                # V4.33: during maint, BLE is off. POST settings over HTTP.
                with state.maintenance_lock:
                    _maint = state.maintenance_state
                if _maint == "ON":
                    try:
                        r = requests.post(
                            f"http://{BIKE_IP}/settings",
                            data=js,
                            headers={"Content-Type": "text/plain"},
                            timeout=5)
                        print(f"[SET-DBG] http write: HTTP {r.status_code} {r.text[:80]}")
                        if r.status_code == 200:
                            set_status("Settings applied (HTTP)")
                            print("[SET-DBG] settings applied, closing dialog")
                            self.root.after(0, dlg.destroy)
                        else:
                            body = r.text[:200]
                            self.root.after(0, lambda b=body: messagebox.showerror("Settings", f"Device rejected settings.\n{b}"))
                    except Exception as exc:
                        msg = f"Write failed:\n{exc}"
                        self.root.after(0, lambda m=msg: messagebox.showerror("Settings", m))
                    return
                async def _do():
                    c = self.worker.client
                    ack_event = asyncio.Event()
                    ack = {"code": None}
                    def on_ack(sender, data):
                        b = bytes(data)
                        if len(b) >= 1:
                            ack["code"] = b[0]
                            ack_event.set()
                    await c.start_notify(SETTINGS_UUID, on_ack)
                    await asyncio.sleep(0.2)
                    await c.write_gatt_char(SETTINGS_UUID, js.encode())
                    try:
                        await asyncio.wait_for(ack_event.wait(), timeout=5)
                    except asyncio.TimeoutError:
                        try:
                            await c.stop_notify(SETTINGS_UUID)
                        except Exception:
                            pass
                        return ("timeout", None)
                    try:
                        await c.stop_notify(SETTINGS_UUID)
                    except Exception:
                        pass
                    return ("ok" if ack["code"] == 0x01 else "fail", ack["code"])
                try:
                    fut = asyncio.run_coroutine_threadsafe(_do(), self.worker.loop)
                    status, code = fut.result(timeout=10)
                    if status == "ok":
                        set_status("Settings applied (ACK)")
                        print("[SET-DBG] settings applied (BLE ACK), closing dialog")
                        self.root.after(0, dlg.destroy)
                    elif status == "fail":
                        self.root.after(0, lambda: messagebox.showerror("Settings", "Device rejected settings (0x%02X). Check ranges — Exit must be < Enter." % code))
                    else:
                        self.root.after(0, lambda: messagebox.showerror("Settings", "No ACK from device (timeout)."))
                except Exception as exc:
                    msg = f"Write failed:\n{exc}"
                    self.root.after(0, lambda m=msg: messagebox.showerror("Settings", m))
            threading.Thread(target=_write, daemon=True).start()
        tk.Button(btn_row, text="Cancel", command=dlg.destroy, width=10,
                  font=("Helvetica", 11)).pack(side="left", padx=8)
        tk.Button(btn_row, text="Apply", command=apply, width=10,
                  font=("Helvetica", 11)).pack(side="left", padx=8)

        # V4.43: theme the settings dialog to match the current theme.
        t = self.theme
        def theme_walk(w):
            try:
                cls = w.winfo_class()
            except Exception:
                return
            try:
                if cls in ("Frame", "Toplevel"):
                    w.configure(bg=t["bg"])
                elif cls == "Label":
                    w.configure(bg=t["bg"], fg=t["fg"])
                elif cls == "Entry":
                    w.configure(bg=t["card"], fg=t["fg"],
                                insertbackground=t["fg"],
                                highlightbackground=t["card_border"])
                elif cls == "Button":
                    w.configure(bg=t["card"], fg=t["fg"],
                                activebackground=t["accent"],
                                activeforeground=t["fg"])
            except Exception:
                pass
            for c in w.winfo_children():
                theme_walk(c)
        theme_walk(dlg)

    def tick(self):
        # V4.33: auto-detect maint from the bike side. Covers entering
        # maint by any path (post-OTA boot, engine-start bail recovery,
        # GUI restart mid-maint) without restarting the GUI.
        if not hasattr(self, "_maint_probe_fail"):
            self._maint_probe_fail = 0
            self._maint_probe_last = 0
        now = time.time()
        if now - self._maint_probe_last > 15:
            self._maint_probe_last = now
            def _probe():
                import urllib.request
                try:
                    with urllib.request.urlopen(
                            f"http://{BIKE_IP}/version", timeout=2) as r:
                        up = (r.status == 200)
                except Exception:
                    up = False
                def _apply():
                    with state.maintenance_lock:
                        cur = state.maintenance_state
                        if up and cur == "OFF":
                            state.maintenance_state = "ON"
                            print("[MAINT] probe: server up, state -> ON")
                            self._maint_probe_fail = 0
                        elif not up and cur == "ON":
                            self._maint_probe_fail += 1
                            if self._maint_probe_fail >= 2:
                                state.maintenance_state = "OFF"
                                print("[MAINT] probe: server down twice, state -> OFF")
                                self._maint_probe_fail = 0
                        else:
                            self._maint_probe_fail = 0
                self.root.after(0, _apply)
            threading.Thread(target=_probe, daemon=True).start()
        d = state.latest_data
        v = d.get("v"); t = d.get("t"); st = d.get("s", "--")
        stale = (time.time() - state.latest_seen_time) > BLE_STALE_SEC

        # Voltage — no fake default
        if v is None or stale:
            self.volt_lbl.config(text="--.-- V", fg=MUTED)
        else:
            if v >= 13.8: c, ve = GREEN, "⚡"
            elif v >= 12.5: c, ve = GREEN, "🔋"
            elif v >= 12.0: c, ve = YELLOW, "⚠️"
            else: c, ve = RED, "🆘"
            self.volt_lbl.config(text=f"{ve} {v:.2f} V", fg=c)

        # State — NO SIGNAL when stale, otherwise the reported state
        if stale or st == "--":
            self.state_lbl.config(text="❓ NO SIGNAL", fg=MUTED)
        else:
            em = {"RUNNING": "🟢", "MONITOR": "💤", "SLEEP": "💤", "PANIC": "🚨"}.get(st, "")
            self.state_lbl.config(text=f"{em} {st}")
            if st == "RUNNING": self.state_lbl.config(fg=GREEN)
            elif st == "PANIC": self.state_lbl.config(fg=RED)
            elif st == "MONITOR": self.state_lbl.config(fg=BLUE)
            else: self.state_lbl.config(fg=FG)
        with state.maintenance_lock:
            m = state.maintenance_state
        if not hasattr(self, "_last_maint_log") or self._last_maint_log != m:
            self._last_maint_log = m
            print(f"[MAINT] tick sees state={m}")
        self._update_maint_menu()
        if m == "OFF":
            if self.maint_lbl.winfo_ismapped():
                self.maint_lbl.pack_forget()
        elif m == "PENDING":
            # V4.29: show a countdown so the user knows the bike is
            # asleep, not dead. Day wait: ~7 min. Night wait: ~12 min.
            elapsed = time.time() - state.maint_pending_since if state.maint_pending_since else 0
            timeout = state.maint_pending_timeout or 660
            remaining = max(0, int(timeout - elapsed))
            mm, ss = divmod(remaining, 60)
            self.maint_lbl.config(
                text=f"🔧  MAINT PENDING  {mm}:{ss:02d}  (bike asleep)",
                bg="#ff8c00", fg="#000000")
            if not self.maint_lbl.winfo_ismapped():
                self.maint_lbl.pack(pady=(0, 8))
        elif m == "ON":
            self.maint_lbl.config(text="🔧  MAINTENANCE: ON",
                                  bg="#00c853", fg="#000000")
            if not self.maint_lbl.winfo_ismapped():
                self.maint_lbl.pack(pady=(0, 8))
        if t is None or stale:
            self.temp_lbl.config(text="--.-C", fg=MUTED)
        else:
            if t < 0: te = "❄️"
            elif t < 10: te = "🥶"
            elif t < 20: te = "🌤"
            elif t < 30: te = "☀️"
            else: te = "☀️"
            self.temp_lbl.config(text=f"{te} {t:.1f}C")
        self.time_lbl.config(text=f"{time_emoji()} {local_time_str()}")
        self.acc_lbl.config(text="ON" if d.get("a") else "OFF",
                            fg=GREEN if d.get("a") else MUTED)
        self.eng_lbl.config(text="RUN" if d.get("e") else "PARK",
                            fg=GREEN if d.get("e") else MUTED)
        self.warn_lbl.config(text="YES" if d.get("w") else "NO",
                             fg=RED if d.get("w") else MUTED)
        self.weather_label.config(
            text=f"Atkinsons Dam · {state.weather['temp']:.1f}° {state.weather['emoji']}")
        fw = state.latest_data.get("fv", "?")
        self.footer_lbl.config(text=f"GUI v{GUI_VERSION}  ·  FW v{fw}")
        with state.latest_ride_lock:
            r = state.latest_ride
        if r and r["summary"]:
            s = r["summary"]
            start = datetime.fromtimestamp(s["startEpoch"])
            self.last_ride_lbl.config(text=start.strftime("%a %d %b, %I:%M%p"), fg=FG)
            if s["flags"] == 0:
                sub_color = GREEN
            elif s["flags"] & 0x30 or s["flags"] & 0x08:
                sub_color = RED
            else:
                sub_color = YELLOW
            self.last_ride_sub.config(
                text=f"{ago_str(s['startEpoch'])}  ·  {s['durationSecs']//60} min",
                fg=sub_color)
            pre = s.get("preRideVolt", 0)
            if pre > 0:
                self.last_ride_pre.config(
                    text=f"cold {pre:.2f}V  →  ride {s['avgVolt']:.2f}V",
                    fg=YELLOW)
            else:
                self.last_ride_pre.config(text="", fg=MUTED)
        else:
            self.last_ride_lbl.config(text="none yet", fg=MUTED)
            self.last_ride_sub.config(text="", fg=MUTED)
            self.last_ride_pre.config(text="", fg=MUTED)
        self.volt_graph.set_data(state.volt_hist)
        self.temp_graph.set_data(state.temp_hist)
        self.root.after(1000, self.tick)
