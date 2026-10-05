#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
bridge = (ROOT / "src/emulators/flip_libretro_core.c").read_text()
gambatte = (ROOT / "src/emulators/flip_gambatte_core.c").read_text()
mgba = (ROOT / "src/emulators/flip_mgba_core.c").read_text()
bios_header = (ROOT / "include/flip_bios.h").read_text()
bios_source = (ROOT / "src/runtime/flip_bios.c").read_text()
makefile = (ROOT / "Makefile").read_text()

errors = []

for token in ['#include "flip_bios.h"', "flip_bios_system_directory("]:
    if token not in bridge:
        errors.append(f"shared Libretro bridge must consume BIOS Manager: {token}")

for token in ['.bios_system_id = "GB"', 'flip_libretro_core_adapter(&config)']:
    if token not in gambatte:
        errors.append(f"Gambatte BIOS configuration missing: {token}")

for token in ['.bios_system_id = "GBA"', 'flip_libretro_core_adapter(&config)']:
    if token not in mgba:
        errors.append(f"mGBA BIOS configuration missing: {token}")

for token in [
    '"/mnt/SDCARD/flipos-dev/system/gb"',
    '"/mnt/SDCARD/flipos-dev/system/gba"',
    '"gb_bios.bin"',
    '"gbc_bios.bin"',
    '"gba_bios.bin"',
]:
    if token in bridge:
        errors.append(f"shared Libretro bridge must not own BIOS policy: {token}")

for token in [
    "FLIP_BIOS_GB_DMG", "FLIP_BIOS_GB_CGB", "FLIP_BIOS_GBA",
    '"gb_bios.bin"', '"gbc_bios.bin"', '"gba_bios.bin"',
    "256", "2304", "16384",
]:
    if token not in bios_header + bios_source:
        errors.append(f"BIOS Manager missing metadata/contract: {token}")

for token in [
    "$(BUILD)/flip_bios.o",
    "$(MAKE) test-bios",
    "python3 tests/test_bios_architecture.py",
    "$(BUILD)/test-flip-bios",
]:
    if token not in makefile:
        errors.append(f"BIOS Manager verification/build wiring missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("BIOS architecture tests passed")
