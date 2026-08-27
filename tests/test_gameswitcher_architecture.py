#!/usr/bin/env python3

from pathlib import Path

header = Path("include/flip_gameswitcher_ui.h")
source = Path("src/frontend/flip_gameswitcher_ui.c")

if not header.exists():
    print("missing dedicated GameSwitcher UI header")
    raise SystemExit(1)

if not source.exists():
    print("missing dedicated GameSwitcher UI implementation")
    raise SystemExit(1)

header_text = header.read_text()
source_text = source.read_text()

required_header_tokens = [
    "FLIP_GAMESWITCHER_BACK",
    "FLIP_GAMESWITCHER_NEW_GAME",
    "FLIP_GAMESWITCHER_RESUME",
    "FLIP_GAMESWITCHER_CLOSE",
    "FLIP_GAMESWITCHER_ERROR",
    "flip_gameswitcher_result_t",
    "flip_gameswitcher_run(",
]

for token in required_header_tokens:
    if token not in header_text:
        print(f"missing GameSwitcher public interface token: {token}")
        raise SystemExit(1)

if "flip_session_manager_refresh(" not in source_text:
    print("GameSwitcher UI must query Session Manager refresh API")
    raise SystemExit(1)

for token in [
    "recent_roms",
    "load_recent_history",
    "RECENT_FILE",
    "history",
]:
    if token in source_text:
        print(f"GameSwitcher UI depends on Recent data: {token}")
        raise SystemExit(1)

for token in [
    "FLIP_SESSION_MANAGER_ROOT",
    ".session",
    "fnv1a",
    ".sav",
    ".state",
    ".restore-pending",
]:
    if token in source_text:
        print(
            "GameSwitcher UI crosses persistence boundary: "
            f"{token}"
        )
        raise SystemExit(1)



frontend = Path("src/frontend/flip_frontend.c").read_text()

marker = 'FRONTEND_GAMESWITCHER entered'
assert marker in frontend

start = frontend.find(marker)
window_start = max(0, start - 1800)
window_end = min(len(frontend), start + 1800)
gameswitcher_window = frontend[window_start:window_end]

for forbidden in [
    "load_recent_history(",
    "recent_selected",
    "select_suspended_recent(",
    "draw_recent(",
    "SCREEN_RECENT",
]:
    if forbidden in gameswitcher_window:
        print(
            "frontend GameSwitcher still depends on Recent: "
            f"{forbidden}"
        )
        raise SystemExit(1)

if "flip_gameswitcher_run(" not in frontend:
    print(
        "frontend does not delegate GameSwitcher UI "
        "to flip_gameswitcher_run"
    )
    raise SystemExit(1)


interactive_requirements = [
    '#include "flip_draw.h"',
    '#include "flip_input.h"',
    "flip_draw_clear(",
    "flip_draw_text(",
    "flip_drm_present(",
    "flip_input_poll(",
    "FLIP_BTN_UP",
    "FLIP_BTN_DOWN",
    "FLIP_BTN_A",
    "FLIP_BTN_B",
    '"+ NEW GAME"',
]

for token in interactive_requirements:
    if token not in source_text:
        print(
            "GameSwitcher interactive shell missing: "
            f"{token}"
        )
        raise SystemExit(1)


close_action_requirements = [
    "FLIP_BTN_X",
    "FLIP_GAMESWITCHER_CLOSE",
    "X CLOSE",
]

for token in close_action_requirements:
    if token not in source_text:
        print(
            "GameSwitcher close action missing: "
            f"{token}"
        )
        raise SystemExit(1)

print("gameswitcher architecture boundary passed")
