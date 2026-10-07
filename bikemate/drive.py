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
        for sub in ("wakes", "rides", "bin", "dyna", "other"):
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


GH_FOLDERS = ("WAKES", "RIDES", "BIN", "DYNA")


def _gh_headers():
    return {
        "Authorization": f"Bearer {GH_TOKEN}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "bike-mate-gui",
    }


def fetch_drive_list():
    # GitHub Contents API: one call per subfolder.
    out = []
    for folder in GH_FOLDERS:
        url = f"{GH_API}/repos/{GH_REPO}/contents/{folder}?ref={GH_BRANCH}"
        try:
            req = urllib.request.Request(url, headers=_gh_headers())
            with urllib.request.urlopen(req, timeout=30, context=state.SSL_CTX) as r:
                items = json.loads(r.read().decode())
            for it in items:
                if it.get("type") != "file":
                    continue
                out.append({
                    "name":   it["name"],
                    "folder": folder,
                    "size":   it.get("size", 0),
                    "url":    it.get("download_url"),
                })
        except urllib.error.HTTPError as e:
            if e.code == 404:
                # Folder does not exist yet on GitHub — skip.
                continue
            print(f"[SYNC] list {folder} failed: {e}")
            return None
        except Exception as e:
            print(f"[SYNC] list {folder} failed: {e}")
            return None
    return out


def categorize(name):
    if name.startswith("wakes_"):
        return "wakes"
    if name.startswith("ride_"):
        return "rides"
    if name.startswith("dyna_"):
        return "dyna"
    if name.startswith("bike_mate_") and name.endswith(".bin"):
        return "bin"
    return "other"


def sync_from_drive():
    files = fetch_drive_list()
    if files is None:
        return -1, 0

    local = set()
    for sub in ("wakes", "rides", "bin", "dyna", "other"):
        d = os.path.join(DRIVE_DIR, sub)
        if os.path.isdir(d):
            for n in os.listdir(d):
                local.add(n)
                # V4.35: .gz files are stored locally verbatim, name
                # preserved. No decode, no rename.

    # V4.35: skip firmware .bin files on sync. They already live in
    # the OTA archive on the Mac and in BIN/ on GitHub; no need for a
    # third copy. BIN/ is still versioned and browsable on GitHub.
    missing = [f for f in files if f.get("name")
               and not f["name"].endswith(".bin")
               and f["name"] not in local]

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
        url = f.get("url")
        if not url:
            print(f"[SYNC] no download_url for {name}")
            continue
        try:
            req = urllib.request.Request(url, headers=_gh_headers())
            with urllib.request.urlopen(req, timeout=60, context=state.SSL_CTX) as r:
                data = r.read()
            with open(dest, "wb") as fh:
                fh.write(data)
            dl += 1
            print(f"[SYNC] {dl}/{len(missing)} {name} ({len(data)} bytes)")
        except Exception as e:
            print(f"[SYNC] download failed {name}: {e}")

    print(f"[SYNC] downloaded {dl} of {len(missing)}")
    return dl, len(missing)


def backup_firmware_to_drive():
    # GitHub Contents API PUT into BIN/.
    try:
        ver = read_firmware_version() or "unknown"
        fname = f"bike_mate_{ver}.bin"
        with open(BUILD_BIN, "rb") as f:
            raw = f.read()
        b64 = base64.b64encode(raw).decode()
        payload = json.dumps({
            "message": f"firmware backup {ver}",
            "content": b64,
            "branch":  GH_BRANCH,
        }).encode()
        url = f"{GH_API}/repos/{GH_REPO}/contents/BIN/{fname}"
        req = urllib.request.Request(url, data=payload, method="PUT",
                                     headers={**_gh_headers(),
                                              "Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=120, context=state.SSL_CTX) as r:
            resp = json.loads(r.read().decode())
        print(f"[BACKUP] {resp.get('content', {}).get('path', resp)}")
        return True
    except urllib.error.HTTPError as e:
        body = e.read().decode(errors="replace")[:200]
        print(f"[BACKUP] HTTP {e.code}: {body}")
        return False
    except Exception as e:
        print(f"[BACKUP] failed: {e}")
        return False

