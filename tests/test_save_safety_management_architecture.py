#!/usr/bin/env python3
from pathlib import Path

runner = Path("src/runtime/flip_emulator_runner.c").read_text()
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
    "flip_save_query_status(",
    "flip_state_query_status(",
    "flip_recovery_restore_previous(",
    "flip_recovery_undo(",
    "FLIP_SAVE_SAFETY: current=%s previous=%s recovery=%s resume=%s undo_state=%s",
]
for token in required:
    if token not in runner:
        errors.append(f"shared Save Safety management missing: {token}")

for token in [
    '".sav"', '".sav.prev"', '".sav.recovery"',
    '".state"', '".state.recovery"', '".restore-pending"',
    "FLIP_SAVE_ROOT", "FLIP_STATE_ROOT", "FLIP_SESSION_RECOVERY_ROOT",
]:
    if token in runner:
        errors.append(f"shared runner must not own persistence path details: {token}")

start = runner.find("static int run_save_safety_management(")
end = runner.find("\nint flip_emulator_run(", start)
body = runner[start:end if end >= 0 else len(runner)]
if "flip_core_host_run(" in body:
    errors.append("Save Safety management mode must not enter gameplay Core Host")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("shared Save Safety management architecture passed")
