#!/usr/bin/env python3

from pathlib import Path

PATH = Path("src/emulators/flip_gb.c")

text = PATH.read_text()

forbidden = [
    "retro_run(",
    "retro_set_environment(",
    "retro_set_video_refresh(",
    "retro_set_audio_sample(",
    "retro_set_input_state(",
    "flip_input_poll(",
    "flip_controls_update(",
    "flip_timing_wait_frame(",
    "flip_lifecycle_handle(",
    "RETRO_DEVICE_ID_JOYPAD_",
    "flip_core_serialize_size(",
    "flip_core_serialize(",
    "flip_core_unserialize(",
]

required = [
    '#include "flip_core.h"',
    '#include "flip_core_host.h"',
    '#include "flip_gambatte_core.h"',
    '#include "flip_state.h"',
    "flip_gambatte_core_adapter()",
    "flip_core_host_run(",
    "flip_state_context_init(",
    "flip_state_save(",
    "flip_state_restore(",
]

errors = []

for token in forbidden:
    if token in text:
        errors.append(f"forbidden host/core detail remains: {token}")

for token in required:
    if token not in text:
        errors.append(f"required composition token missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("flip_gb architecture boundary passed")
