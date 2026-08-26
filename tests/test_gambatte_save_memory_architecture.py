#!/usr/bin/env python3

from pathlib import Path

PATH = Path("src/emulators/flip_gambatte_core.c")
text = PATH.read_text()

required = [
    "retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)",
    "retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)",
    ".save_memory_data =",
    ".save_memory_size =",
]

errors = []

for token in required:
    if token not in text:
        errors.append(f"required Gambatte battery-memory token missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("gambatte save-memory architecture test passed")
