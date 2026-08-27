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



# Reset Game Data must be exposed through Save Safety, but the frontend
# must delegate persistence ownership to the runtime API.
if '"flip_game_data.h"' not in FRONTEND_LAUNCH:
    errors.append(
        "Save Safety frontend missing game-data reset API"
    )

if "flip_game_data_reset(" not in FRONTEND_LAUNCH:
    errors.append(
        "Save Safety frontend missing Reset Game Data action"
    )

for token in [
    "RESET GAME DATA",
    "RESET ALL GAME DATA?",
]:
    if token not in FRONTEND_LAUNCH:
        errors.append(
            f"Save Safety reset confirmation UX missing: {token}"
        )



# A successful game-data reset can remove the current resume state.
# Returning to GAME OPTIONS must therefore refresh resume availability;
# it must not keep the pre-reset can_resume decision.
for token in [
    "reset_performed",
    "int safety_result",
]:
    if token not in FRONTEND_LAUNCH:
        errors.append(
            f"Save Safety reset does not refresh launch options: {token}"
        )

if FRONTEND_LAUNCH.count(
    "flip_system_resume_available("
) < 2:
    errors.append(
        "Save Safety reset does not refresh resume availability"
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


#
# Save Safety and its nested modal loops must participate in the same
# global hardware-control policy as the rest of FlipOS.
#
for token in [
    '"flip_frontend_global_controls.h"',
    "flip_frontend_global_controls_t",
    "flip_frontend_global_controls_init(",
    "flip_frontend_global_controls_handle(",
]:
    if token not in FRONTEND_LAUNCH:
        errors.append(
            f"Save Safety global controls missing: {token}"
        )

#
# There are three interactive loops here:
# confirmation, Save Safety menu, and launch-mode chooser.
# Each must delegate input to the shared global-controls handler.
#
global_control_calls = FRONTEND_LAUNCH.count(
    "flip_frontend_global_controls_handle("
)

if global_control_calls < 3:
    errors.append(
        "Save Safety nested loops do not all use global controls "
        f"(calls={global_control_calls})"
    )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("frontend Save Safety architecture passed")
