#!/usr/bin/env python3

from pathlib import Path

GB = Path("src/emulators/flip_gb.c").read_text()

errors = []

required = [
    '"--save-safety-status"',
    '"--restore-previous-save"',
    '"--undo-last-restore"',
    "run_save_safety_management(",
    "flip_core_init(",
    "flip_core_load_game(",
    "flip_core_unload_game(",
    "flip_core_deinit(",
    "flip_save_health(",
    "flip_state_recovery_health(",
    "flip_recovery_restore_previous(",
    "flip_recovery_undo(",
    "FLIP_SAVE_SAFETY: current=%s previous=%s recovery=%s resume=%s undo_state=%s",
]

for token in required:
    if token not in GB:
        errors.append(f"Save Safety management mode missing: {token}")

forbidden = [
    '".sav"',
    '".sav.prev"',
    '".sav.recovery"',
    '".state"',
    '".state.recovery"',
    '".restore-pending"',
    "FLIP_SAVE_ROOT",
    "FLIP_STATE_ROOT",
    "FLIP_SESSION_RECOVERY_ROOT",
]

for token in forbidden:
    if token in GB:
        errors.append(f"flip_gb management must not own persistence paths: {token}")

start = GB.find("static int run_save_safety_management(")
if start >= 0:
    main = GB.find("\nint main(", start)
    body = GB[start: main if main >= 0 else len(GB)]
    if "flip_core_host_run(" in body:
        errors.append("Save Safety management mode must not enter gameplay Core Host")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("Save Safety management architecture passed")
