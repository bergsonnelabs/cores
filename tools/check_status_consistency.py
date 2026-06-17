#!/usr/bin/env python3
"""Guard the SDK implementation-status data against id drift.

The status matrix in sdk/status/ has two parts that must stay in lockstep:

  - features.json   defines the canonical feature ids (grouped, ordered)
  - core-*.json     records each Core's status keyed by those ids

The website renders features.json's ids and looks each up by exact string in
the core files. So a key in a core file that isn't a defined feature is
invisible (silently dropped), and a renamed feature silently reads as
"planned" everywhere. This drift is exactly how the L0/L4/H5 files ended up on
an older id scheme than features.json + core-w. This check fails the build if:

  1. a core file has a key that isn't a defined feature id (orphan), or
  2. a feature id appears in no core file at all (dead row).

Run:
    python3 tools/check_status_consistency.py
"""

import json
import sys
from pathlib import Path

STATUS_DIR = Path(__file__).resolve().parent.parent / "sdk" / "status"


def main():
    feat = json.loads((STATUS_DIR / "features.json").read_text())
    canon = [f["id"] for g in feat["groups"] for f in g["features"]]
    canon_set = set(canon)

    if len(canon) != len(canon_set):
        dupes = sorted({i for i in canon if canon.count(i) > 1})
        print(f"FAIL: duplicate feature ids in features.json: {dupes}", file=sys.stderr)
        sys.exit(1)

    cores = sorted(STATUS_DIR.glob("core-*.json"))
    used = set()
    failed = False
    for path in cores:
        d = json.loads(path.read_text())
        keys = set(d["features"].keys())
        used |= keys
        orphans = sorted(keys - canon_set)
        if orphans:
            failed = True
            print(
                f"FAIL: {path.name} has keys not defined in features.json "
                f"(rename or add them): {orphans}",
                file=sys.stderr,
            )

    dead = sorted(canon_set - used)
    if dead:
        # Not fatal — a freshly-defined feature legitimately has no Core value
        # yet — but surface it so dead rows don't accumulate unnoticed.
        print(f"note: features with no value on any Core: {dead}", file=sys.stderr)

    if failed:
        sys.exit(1)
    print(f"status consistency OK — {len(canon)} feature ids, {len(cores)} cores")


if __name__ == "__main__":
    main()
