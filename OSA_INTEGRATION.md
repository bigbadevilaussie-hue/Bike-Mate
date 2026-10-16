# Bike-Mate → OSA Server integration

Written 2026-10-16 by the Fan-Mate session, after Fan-Mate's own OSA
integration was reworked end to end. Bike-Mate was the reference
implementation; Fan-Mate is now cleaner in two places.

## Two weaknesses in the current Bike-Mate pusher

### 1. Manifest saved once at end of cycle

`_gh_push_cycle` writes `.pushed.json` after the whole loop finishes.
If OSA restarts mid-cycle, all progress is lost. Fix: save every ~10
pushes, atomically, via tempfile + os.replace.

### 2. 422 is assumed to mean "already exists"

GitHub returns 422 for at least three conditions: file exists without
sha, secondary rate limit, validation error. Current code treats all
three as success. Fix: read the body, distinguish the three cases.

## What Fan-Mate changed

1. Remote git tree is the source of truth (one GET /git/trees at cycle start).
2. Manual push_now coalesced via threading.Event.
3. 1.2s throttle between pushes.

## When to rewrite

Not now. Trigger conditions:
- A cycle pushes more than ~50 files
- A non-file-exists 422 appears in /tmp/osa_server.log
- OSA restarts mid-cycle more than once
- Manifest grows past ~1 MB

Until then, leave `_gh_push_cycle` alone.
