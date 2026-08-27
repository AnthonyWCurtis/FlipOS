#!/usr/bin/env python3

from pathlib import Path

FRONTEND_MAIN = Path("src/frontend/flip_frontend.c").read_text()
FRONTEND_UI = Path("src/frontend/flip_save_safety_ui.c").read_text()
FRONTEND_LAUNCH = Path("src/frontend/flip_save_safety_launch.c").read_text()
FRONTEND = FRONTEND_UI + "\n" + FRONTEND_LAUNCH
SESSION_HEADER = Path("include/flip_session.h").read_text()
HEADER = Path("include/flip_session_adapter.h").read_text()
ADAPTER = Path("src/runtime/flip_session_adapter.c").read_text()
MAKEFILE = Path("Makefile").read_text()

errors = []

required_header = [
    "supports_save_safety",
    "build_start_normal_argv",
    "build_save_safety_argv",
]
for token in required_header:
    if token not in HEADER:
        errors.append(f"session adapter Save Safety capability missing: {token}")

required_adapter = [
    '"--no-resume"',
    '"--save-safety-status"',
    '"--restore-previous-save"',
    '"--undo-last-restore"',
]
for token in required_adapter:
    if token not in ADAPTER:
        errors.append(f"session adapter command mapping missing: {token}")

required_frontend = [
    "RESUME GAME",
    "START NORMALLY",
    "SAVE SAFETY",
    "RESTORE PREVIOUS SAVE",
    "UNDO LAST RESTORE",
    "build_start_normal_argv",
    "build_save_safety_argv",
    "FLIP_SAVE_SAFETY:",
    "flip_save_safety_query(",
    "flip_save_safety_restore_previous(",
    "flip_save_safety_undo(",
    "flip_session_launch_legacy(",
]
for token in required_frontend:
    if token not in FRONTEND:
        errors.append(f"generic frontend Save Safety UX missing: {token}")

# A lid wake is an automatic continuation of the session, not a new user
# launch decision. The frontend must tag that relaunch explicitly and the
# Save Safety wrapper must bypass GAME OPTIONS only for that case.
if "int automatic_resume;" not in SESSION_HEADER:
    errors.append("session metadata missing automatic_resume intent")
if "automatic_resume" not in FRONTEND_MAIN:
    errors.append("frontend lid relaunch does not carry automatic_resume intent")
if "metadata->automatic_resume" not in FRONTEND_LAUNCH:
    errors.append("Save Safety wrapper does not bypass menu for automatic resume")


# A retained GameSwitcher session is also an explicit continuation,
# but it must remain semantically distinct from lid automatic resume.
if "int gameswitcher_resume;" not in SESSION_HEADER:
    errors.append(
        "session metadata missing gameswitcher_resume intent"
    )

if (
    "launch_game(" not in FRONTEND_MAIN or
    "&resume_rom,\n                    1" not in FRONTEND_MAIN
):
    errors.append(
        "GameSwitcher resume does not set explicit resume intent"
    )

if "metadata->gameswitcher_resume" not in FRONTEND_LAUNCH:
    errors.append(
        "Save Safety wrapper does not bypass menu for GameSwitcher resume"
    )

required_makefile = [
    "flip_save_safety_ui.o",
    "flip_save_safety_launch.o",
    "-Dflip_session_launch=flip_session_launch_legacy",
]
for token in required_makefile:
    if token not in MAKEFILE:
        errors.append(f"Save Safety frontend build wiring missing: {token}")

# The frontend may use generic session.state metadata; it must not know
# emulator persistence filenames, roots, hashes or marker names.
forbidden_frontend = [
    '".sav"',
    '".sav.prev"',
    '".sav.recovery"',
    '".state.recovery"',
    '".restore-pending"',
    "FLIP_SAVE_ROOT",
    "FLIP_STATE_ROOT",
    "FLIP_SESSION_RECOVERY_ROOT",
    "792f9628ba43b7d9",
    "/mnt/SDCARD/flipos-dev/saves/",
    "/mnt/SDCARD/flipos-dev/states/",
]
for token in forbidden_frontend:
    if token in FRONTEND_MAIN or token in FRONTEND:
        errors.append(f"frontend must not own persistence storage detail: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("frontend Save Safety architecture passed")
