#!/usr/bin/env python3

from pathlib import Path

header = Path("include/flip_game_data.h")
runtime = Path("src/runtime/flip_game_data.c")
frontend = Path("src/frontend/flip_save_safety_launch.c")
makefile = Path("Makefile").read_text()

errors = []

if not header.exists():
    errors.append("missing game-data reset public API")

if not runtime.exists():
    errors.append("missing game-data reset runtime implementation")

frontend_text = frontend.read_text()

for forbidden in [
    ".sav",
    ".sav.prev",
    ".sav.recovery",
    ".state",
    ".state.prev",
    ".state.recovery",
    ".restore-pending",
    "unlink(",
    "rename(",
    "FLIP_SAVE_ROOT",
    "FLIP_STATE_ROOT",
]:
    if forbidden in frontend_text:
        errors.append(
            f"frontend must not own game-data filesystem detail: {forbidden}"
        )

required_make = [
    "$(BUILD)/flip_game_data.o",
    "test-game-data-reset",
]

for token in required_make:
    if token not in makefile:
        errors.append(
            f"game-data reset build/test wiring missing: {token}"
        )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("game-data reset architecture passed")
