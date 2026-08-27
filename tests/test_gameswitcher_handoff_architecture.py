#!/usr/bin/env python3

from pathlib import Path

frontend = Path("src/frontend/flip_frontend.c").read_text()

launch_start = frontend.index("static int launch_game(")
launch_end = frontend.index("\nint main(void)", launch_start)
launch_game = frontend[launch_start:launch_end]

if "FLIP_SESSION_ACTION_SUSPEND" not in launch_game:
    print("launch_game no longer classifies suspend handoffs")
    raise SystemExit(1)

if "wait_for_lid_open(" not in launch_game:
    print("lid auto-resume path missing from launch_game")
    raise SystemExit(1)

if "automatic_resume = 1;" not in launch_game:
    print("automatic lid resume intent missing")
    raise SystemExit(1)

record_token = "flip_session_manager_record_gameswitcher("

if record_token not in launch_game:
    print("GameSwitcher handoff is not recorded in launch_game")
    raise SystemExit(1)

gameswitcher_pos = launch_game.index(
    "FRONTEND_GAMESWITCHER requested"
)
record_pos = launch_game.index(record_token)
wait_pos = launch_game.index("wait_for_lid_open(")
auto_resume_pos = launch_game.index("automatic_resume = 1;")

if not gameswitcher_pos < record_pos < wait_pos:
    print(
        "retained-session record must occur only in "
        "lid-open GameSwitcher branch"
    )
    raise SystemExit(1)

lid_resume_slice = launch_game[wait_pos:auto_resume_pos + 80]

if record_token in lid_resume_slice:
    print("lid suspend/automatic resume must not record retained session")
    raise SystemExit(1)

print("gameswitcher handoff architecture boundary passed")
