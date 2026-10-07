# === Bike-Mate GUI: drive ===
# auto-extracted, edit here ===

import os, subprocess, gzip, urllib.request, urllib.parse, ssl, base64, shutil, json
from tkinter import messagebox
from .config import *
from . import state
from .helpers import *


drive_server_proc = None


def start_drive_server():
    global drive_server_proc
    if drive_server_proc is not None:
        return drive_server_proc
    try:
        os.makedirs(DRIVE_DIR, exist_ok=True)
        for sub in ("wakes", "rides", "other"):
            os.makedirs(os.path.join(DRIVE_DIR, sub), exist_ok=True)
        drive_server_proc = subprocess.Popen(
            ["python3", "-m", "http.server", str(DRIVE_PORT),
             "--directory", DRIVE_DIR, "--bind", "0.0.0.0"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f"[DRIVE] server started on port {DRIVE_PORT}, dir {DRIVE_DIR}")
    except Exception as e:
        print(f"[DRIVE] server start failed: {e}")
    return drive_server_proc


def stop_drive_server():
    global drive_server_proc
    if drive_server_proc:
        try:
            drive_server_proc.terminate()
        except Exception:
            pass
        drive_server_proc = None


def fetch_drive_list():
    try:
        r = urllib.request.urlopen(UPLOAD_URL + "?action=list",
                                   timeout=30, context=state.SSL_CTX)
        return json.loads(r.read().decode())
    except Exception as e:
        print(f"[SYNC] list failed: {e}")
        return None


def categorize(name):
    if name.startswith("wakes_"):
        return "wakes"
    if name.startswith("ride_"):
        return "rides"
    return "other"


def sync_from_drive():
    files = fetch_drive_list()
    if files is None:
        return -1, 0

    local = set()
    for sub in ("wakes", "rides", "other"):
        d = os.path.join(DRIVE_DIR, sub)
        if os.path.isdir(d):
            for n in os.listdir(d):
                local.add(n)
                # V4.34: a .gz on Drive is stored locally under its
                # decompressed name. Treat both as present so we don't
                # re-download every sync.
                if n.endswith(".gz"):
                    local.add(n[:-3])

    missing = [f for f in files if f.get("name") and f["name"] not in local]

    print(f"[SYNC] {len(files)} in Drive, {len(local)} local, {len(missing)} new")

    if len(missing) == 0:
        return 0, 0

    msg = f"{len(missing)} files to come. Sync?"
    if not messagebox.askyesno("Sync from Drive", msg):
        return -1, -1

    dl = 0
    for f in missing:
        name = f["name"]
        sub = categorize(name)
        dest = os.path.join(DRIVE_DIR, sub, name)
        url = UPLOAD_URL + "?action=download&name=" + urllib.parse.quote(name)
        try:
            r = urllib.request.urlopen(url, timeout=60, context=state.SSL_CTX)
            data = r.read()
            # V4.34: Apps Script returns binary as "B64:<base64>".
            if data.startswith(b"B64:"):
                data = base64.b64decode(data[4:])
            # V4.34: the bike base64-encodes the gzipped payload before
            # POST. Drive stores that text verbatim, so a .csv.gz on
            # Drive is base64 text whose decoded form is gzip whose
            # decompressed form is CSV. Unwrap both on sync so local
            # files are always plain CSV.
            if name.endswith(".gz"):
                try:
                    # V4.34: strip whitespace/non-b64 chars before decode
                    # (Apps Script inserts spaces into long payloads).
                    import re as _re
                    cleaned = _re.sub(rb"[^A-Za-z0-9+/=]", b"", data)
                    decoded = base64.b64decode(cleaned, validate=False)
                    if decoded[:2] == b"\x1f\x8b":
                        data = gzip.decompress(decoded)
                        dest = dest[:-3]  # drop .gz
                except Exception as e:
                    print(f"[SYNC] decode failed {name}: {e}")
            with open(dest, "wb") as fh:
                fh.write(data)
            dl += 1
            print(f"[SYNC] {dl}/{len(missing)} {name} ({len(data)} bytes)")
        except Exception as e:
            print(f"[SYNC] download failed {name}: {e}")

    print(f"[SYNC] downloaded {dl} of {len(missing)}")
    return dl, len(missing)


def backup_firmware_to_drive():
    try:
        ver = read_firmware_version() or "unknown"
        with open(BUILD_BIN, "rb") as f:
            raw = f.read()
        b64 = base64.b64encode(raw).decode()
        body = urllib.parse.urlencode({
            "filename": f"bike_mate_{ver}.bin",
            "binary": "1",
            "data": b64,
        }).encode()
        req = urllib.request.Request(UPLOAD_URL, data=body,
                                     headers={"Content-Type": "application/x-www-form-urlencoded"})
        r = urllib.request.urlopen(req, timeout=120, context=state.SSL_CTX)
        resp = r.read().decode()
        print(f"[BACKUP] {resp[:120]}")
        return resp.startswith("OK")
    except Exception as e:
        print(f"[BACKUP] failed: {e}")
        return False

