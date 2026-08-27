#!/usr/bin/env python3

from pathlib import Path

frontend = Path(
    "src/frontend/flip_frontend.c"
).read_text()

gameswitcher = Path(
    "src/frontend/flip_gameswitcher_ui.c"
).read_text()

errors = []

#
# The existing main frontend must retain its proven global hotkey
# policy while modal screens migrate onto the reusable boundary.
#
if "flip_hotkeys_handle(" not in frontend:
    errors.append(
        "main frontend lost global hotkey handling"
    )

global_header = Path(
    "include/flip_frontend_global_controls.h"
).read_text()

global_source = Path(
    "src/frontend/flip_frontend_global_controls.c"
).read_text()

for token in [
    "flip_frontend_global_controls_init",
    "flip_frontend_global_controls_handle",
]:
    if token not in global_header or token not in global_source:
        errors.append(
            f"shared global-controls boundary missing: {token}"
        )

#
# GameSwitcher owns its own input loop. It must therefore explicitly
# delegate each poll to the same shared global-controls boundary.
#
if "flip_frontend_global_controls" not in gameswitcher:
    errors.append(
        "GameSwitcher does not delegate to global controls"
    )

#
# Global policy must include clean power-off handling as well as
# volume/brightness hotkeys.
#
for token in [
    "FLIP_BTN_MENU",
    "FLIP_BTN_SELECT",
    "flip_hotkeys_handle(",
]:
    if token not in frontend:
        errors.append(
            f"global controls missing policy token: {token}"
        )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("frontend global controls architecture passed")
