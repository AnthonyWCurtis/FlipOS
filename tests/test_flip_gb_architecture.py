#!/usr/bin/env python3
from pathlib import Path

gb = Path("src/emulators/flip_gb.c").read_text()
runner = Path("src/runtime/flip_emulator_runner.c").read_text()

errors = []

for token in [
    '#include "flip_emulator_runner.h"',
    '#include "flip_gambatte_core.h"',
    'flip_gambatte_core_adapter()',
    'flip_emulator_run(&config, argc, argv)',
    '.storage_id = "gb"',
]:
    if token not in gb:
        errors.append(f"thin GB launcher missing: {token}")

for token in [
    "flip_video_",
    "flip_audio_",
    "flip_input_",
    "flip_controls_",
    "flip_timing_",
    "flip_lifecycle_",
    "retro_",
    "flip_save_",
    "flip_state_",
    "flip_recovery_",
]:
    if token in gb:
        errors.append(f"GB launcher must not own host/persistence detail: {token}")

for token in [
    "flip_core_host_run(",
    "flip_save_context_init(",
    "flip_save_load(",
    "flip_save_commit(",
    "flip_state_context_init(",
    "flip_state_save(",
    "flip_state_restore(",
    "flip_recovery_context_init(",
    "flip_recovery_resume_blocked(",
    ".persistence_complete = persistence_complete",
]:
    if token not in runner:
        errors.append(f"shared emulator runner missing composition token: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("GB thin-launcher/shared-runner architecture passed")
