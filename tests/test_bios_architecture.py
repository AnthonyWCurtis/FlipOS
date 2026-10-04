#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

gambatte = (ROOT / "src/emulators/flip_gambatte_core.c").read_text()
bios_header = (ROOT / "include/flip_bios.h").read_text()
bios_source = (ROOT / "src/runtime/flip_bios.c").read_text()
makefile = (ROOT / "Makefile").read_text()

errors = []

required_gambatte = [
    '#include "flip_bios.h"',
    "flip_bios_system_directory(",
]

for token in required_gambatte:
    if token not in gambatte:
        errors.append(f"Gambatte must consume BIOS Manager: {token}")

forbidden_gambatte = [
    '"/mnt/SDCARD/flipos-dev/system/gb"',
    '"gb_bios.bin"',
    '"gbc_bios.bin"',
    '"gambatte_gb_bootloader"',
]

for token in forbidden_gambatte:
    if token in gambatte:
        errors.append(f"Gambatte adapter must not own BIOS policy: {token}")

required_bios = [
    "FLIP_BIOS_GB_DMG",
    "FLIP_BIOS_GB_CGB",
    '"gb_bios.bin"',
    '"gbc_bios.bin"',
    "256",
    "2304",
]

for token in required_bios:
    if token not in bios_header + bios_source:
        errors.append(f"BIOS Manager missing metadata/contract: {token}")

required_make = [
    "$(BUILD)/flip_bios.o",
    "$(MAKE) test-bios",
    "python3 tests/test_bios_architecture.py",
    "$(BUILD)/test-flip-bios",
]

for token in required_make:
    if token not in makefile:
        errors.append(f"BIOS Manager verification/build wiring missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("BIOS architecture tests passed")
