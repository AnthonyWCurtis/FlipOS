#!/usr/bin/env python3
"""Source review guard: USB file transfer must remain opt-in in FlipOS."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
def read(p): return (ROOT / p).read_text()
errors = []

def require(ok, description):
    if not ok:
        errors.append(description)

updater = read("tools/device/updater-playtest")
dev = read("tools/device/deploy-playtest")
frontend = read("src/frontend/flip_frontend.c")
mode = read("tools/device/flip-usb-mode")
require('USB_MODE_SERVICE="$ROOT/bin/flip-usb-mode"' in updater,
        "boot does not select USB mode status helper")
require('"$USB_MODE_SERVICE" boot' in updater, "boot does not use read-only boot check")
require('"$USB_MTP_SERVICE"' not in updater,
        "boot still invokes legacy MTP auto-activation")
require('"$ADB" push tools/device/flip-usb-mode' in dev,
        "development deployment does not include mode helper")
require("SCREEN_SETTINGS_USB" in frontend, "no USB Settings screen")
require('"USB FILE TRANSFER"' in frontend, "USB Settings missing Connectivity menu")
require("usb_mode_confirm = 1;" in frontend and "if (!usb_mode_confirm)" in frontend,
        "USB mode transition is not a two-step confirmation")
require('USB_MODE_COMMAND " mtp"' in frontend and 'USB_MODE_COMMAND " adb"' in frontend,
        "USB Settings does not offer both modes")
require('case "${1:-status}" in' in mode and "boot)" in mode,
        "USB script lacks explicit default/status modes")
require('rm "$MTP_LINK"' in mode, "ADB-only transition does not unlink exact MTP link")
require('rm -f "$CONFIG"/f-*' not in mode,
        "USB mode may remove unrelated gadget function links")
require('flip-usb-mtp' in mode and "guard_transition mtp" in mode,
        "MTP transition lacks guarded opt-in")
require("pidof" in mode and "flip-update-install" in mode,
        "USB mode doesn't check for active gameplay or updater")

if errors:
    for issue in errors: print("USB_MODE_ARCHITECTURE_FAIL:", issue, file=sys.stderr)
    raise SystemExit(1)
print("USB mode architecture proof passed")
