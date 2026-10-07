#!/usr/bin/env python3

from pathlib import Path

source = Path("tools/device/deploy-playtest").read_text()
errors = []

required = [
    "BOOT_UPDATER=/mnt/SDCARD/.tmp_update/updater",
    "$BOOT_UPDATER.previous",
    "$ROOT/updater-playtest",
    "$BOOT_UPDATER",
    "flip-optimizer-supervisor",
    "flip-optimizer-deadman",
    "flip-optimizer-guard",
    "flip-optimizer-calibration",
    "flip-optimizer-candidate",
    "sha256sum",
    "sync",
]

for token in required:
    if token not in source:
        errors.append(f"playtest deploy missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("playtest deploy architecture passed")
