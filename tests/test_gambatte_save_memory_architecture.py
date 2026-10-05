#!/usr/bin/env python3
from pathlib import Path

bridge = Path("src/emulators/flip_libretro_core.c").read_text()
gambatte = Path("src/emulators/flip_gambatte_core.c").read_text()

errors = []

for token in [
    "retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)",
    "retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)",
    ".save_memory_data =",
    ".save_memory_size =",
]:
    if token not in bridge:
        errors.append(f"shared Libretro battery-memory token missing: {token}")

for token in [
    'flip_libretro_core_adapter(&config)',
    '.bios_system_id = "GB"',
    '.storage_id = "gb"',
]:
    if token not in gambatte:
        errors.append(f"thin Gambatte adapter missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("shared Libretro save-memory architecture passed")
