#!/usr/bin/env python3
from pathlib import Path

script = Path("tools/device/flip-usb-mtp").read_text()
config = Path("tools/device/umtprd.conf").read_text()
launcher = Path("tools/device/updater-playtest").read_text()
installer = Path("tools/update/flip-update-install").read_text()
builder = Path("tools/update/build-release-package").read_text()
makefile = Path("Makefile").read_text()

required_script = [
    "/usr/bin/usbdevice stop",
    'bind_dir /mnt/SDCARD/Roms "$MTP_ROOT/ROMs"',
    'bind_dir "$ROOT/system" "$MTP_ROOT/BIOS"',
    'bind_dir "$ROOT/saves" "$MTP_ROOT/Saves"',
    'functions/ffs.mtp',
    'functions/ffs.adb',
    'mount -t functionfs mtp',
    '/usr/bin/adbd',
    'ln -s "$ADB_FUNC" "$ADB_LINK"',
    'ln -s "$MTP_FUNC" "$MTP_LINK"',
    '"FlipOS MTP + ADB"',
]
for token in required_script:
    assert token in script, token

for token in [
    'storage "/mnt/SDCARD/flipos-dev/mtp-root" "FlipOS" "rw"',
    'product "FlipOS"',
    'interface "MTP"',
    'sync_when_close 0x1',
]:
    assert token in config, token

for token in ["flip-usb-mtp", "umtprd", "umtprd.conf", "FLIP_MTP: startup complete"]:
    assert token in launcher, token

assert "etc/umtprd.conf" in installer
assert "build/umtprd" in builder
assert "tools/device/flip-usb-mtp" in builder
assert "tools/device/umtprd.conf" in builder
assert "external/uMTP-Responder/src" in makefile
assert "mtp-responder" in makefile

license_text = Path("external/uMTP-Responder/LICENSE").read_text()
upstream = Path("external/uMTP-Responder/FLIPOS_UPSTREAM").read_text()
assert "GNU GENERAL PUBLIC LICENSE" in license_text
assert "2241a803c27f1fff7537cb9504befeb810b880d3" in upstream

print("USB MTP architecture passed")
