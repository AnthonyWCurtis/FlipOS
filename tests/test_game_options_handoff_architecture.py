from pathlib import Path

frontend = Path(
    "src/frontend/flip_frontend.c"
).read_text()

session_h = Path(
    "include/flip_session.h"
).read_text()

if "FLIP_SESSION_ACTION_GAME_OPTIONS" not in session_h:
    print("distinct Game Options session action missing")
    raise SystemExit(1)

launch_start = frontend.find(
    "static int launch_game("
)

if launch_start < 0:
    print("launch_game flow missing")
    raise SystemExit(1)

launch_end = frontend.find(
    "\nstatic ",
    launch_start + 1
)

if launch_end < 0:
    launch_end = len(frontend)

launch = frontend[
    launch_start:
    launch_end
]

action_pos = launch.find(
    "FLIP_SESSION_ACTION_GAME_OPTIONS"
)

if action_pos < 0:
    print(
        "frontend does not route Game Options handoff"
    )
    raise SystemExit(1)

# Game Options must stay inside launch_game's same-ROM loop.
continue_pos = launch.find(
    "continue;",
    action_pos
)

if continue_pos < 0:
    print(
        "Game Options handoff does not reopen same-ROM launch flow"
    )
    raise SystemExit(1)

# It must not accidentally be treated as GameSwitcher.
window = launch[
    action_pos:
    continue_pos
]

if "record_gameswitcher" in window:
    print(
        "Game Options handoff incorrectly records GameSwitcher"
    )
    raise SystemExit(1)



# In-game Game Options needs a distinct continuation intent.
# Normal library launch must retain its existing cancel/back behavior.
session_h = Path(
    "include/flip_session.h"
).read_text()

save_launch = Path(
    "src/frontend/flip_save_safety_launch.c"
).read_text()

if "game_options_resume" not in session_h:
    print(
        "session metadata cannot distinguish in-game Game Options"
    )
    raise SystemExit(1)

if "metadata->game_options_resume" not in save_launch:
    print(
        "Game Options launch flow ignores in-game resume context"
    )
    raise SystemExit(1)

if "FLIP_SAVE_SAFETY: GAME_OPTIONS_RESUME" not in save_launch:
    print(
        "in-game Game Options resume path lacks explicit diagnostic"
    )
    raise SystemExit(1)

print("game-options handoff architecture passed")
