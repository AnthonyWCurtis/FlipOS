#!/usr/bin/env python3

from pathlib import Path

source = Path("tools/device/deploy-playtest").read_text()
errors = []

required = [
    "BOOT_UPDATER=/mnt/SDCARD/.tmp_update/updater",
    "$BOOT_UPDATER.previous",
    "rm -f '$BOOT_UPDATER.previous'",
    "$ROOT/updater-playtest",
    "$BOOT_UPDATER",
    "flip_rk817_charger.ko",
    "flip-charge-service",
    "umtprd",
    "flip-usb-mtp",
    "umtprd.conf",
    "flip-update-install",
    "flip-update-fetch",
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

assert "state/setup/complete" in source
assert "version=1" in source
assert "first-boot wizard" in source

print("playtest deploy architecture passed")
