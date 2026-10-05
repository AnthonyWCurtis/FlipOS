#!/usr/bin/env python3
from pathlib import Path

launcher = Path("src/emulators/flip_gba.c").read_text()
adapter = Path("src/emulators/flip_mgba_core.c").read_text()
bridge = Path("src/emulators/flip_libretro_core.c").read_text()
runner = Path("src/runtime/flip_emulator_runner.c").read_text()
system = Path("src/runtime/flip_system.c").read_text()
makefile = Path("Makefile").read_text()

errors = []

for token in [
    '#include "flip_emulator_runner.h"',
    '#include "flip_mgba_core.h"',
    'flip_mgba_core_adapter()',
    '.storage_id = "gba"',
    'flip_emulator_run(&config, argc, argv)',
]:
    if token not in launcher:
        errors.append(f"GBA launcher missing thin composition token: {token}")

for token in [
    'flip_libretro_core_adapter(&config)',
    '.bios_system_id = "GBA"',
    '.storage_id = "gba"',
]:
    if token not in adapter:
        errors.append(f"mGBA adapter missing thin configuration token: {token}")

for source_name, text in [("flip_gba.c", launcher), ("flip_mgba_core.c", adapter)]:
    for forbidden in [
        "flip_video_", "flip_audio_", "flip_input_", "flip_controls_",
        "flip_timing_", "flip_lifecycle_", "retro_run(", "retro_set_",
        "flip_save_", "flip_state_", "flip_recovery_",
    ]:
        if forbidden in text:
            errors.append(f"{source_name} duplicates shared host/core detail: {forbidden}")

for token in [
    "video_cb(", "audio_batch_cb(", "input_state_cb(",
    "retro_serialize(", "retro_unserialize(",
    "retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)",
]:
    if token not in bridge:
        errors.append(f"shared Libretro bridge missing: {token}")

for token in ["flip_core_host_run(", "flip_save_load(", "flip_state_restore("]:
    if token not in runner:
        errors.append(f"shared emulator runner missing: {token}")

for token in [
    '.id = "GBA"',
    '"/mnt/SDCARD/flipos-dev/bin/flip-gba"',
    '"/mnt/SDCARD/Roms/GBA"',
    '".gba"',
]:
    if token not in system:
        errors.append(f"GBA system registry missing: {token}")

for token in [
    "gba: $(BUILD)/flip-gba",
    "$(BUILD)/flip-gba:",
    "$(BUILD)/mgba_libretro.a:",
    "-DM_CORE_GBA=ON",
    "-DM_CORE_GB=OFF",
]:
    if token not in makefile:
        errors.append(f"GBA build wiring missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("GBA architecture proof passed")
