#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
progress = (ROOT / "src/hwtest/flip_optimizer_progress.c").read_text()
updater = (ROOT / "tools/device/updater-playtest").read_text()
caltool = (ROOT / "src/hwtest/flip_optimizer_calibration_tool.c").read_text()

errors = []

for token in [
    "SCREEN_SETTINGS",
    "SCREEN_OPTIMIZER",
    "CPU OPTIMIZATION",
    "RUN OPTIMIZATION AGAIN",
    "RESET OPTIMIZATION",
    "optimizer_external_power_online()",
    "battery < 30",
    "OPTIMIZER_UI_RETURN_PATH",
    '"start",\n            "20"',
    '"snapshot"',
    '"reset-for-rerun"',
    '"restore-snapshot"',
    "STRESS TEST PROFILE",
    "OPTIMIZER_VALIDATION_REQUEST_PATH",
    "OPTIMIZER_VALIDATION_RESULT_PATH",
    "optimizer_start_validation()",
    "STRESS TEST OPTIMIZED PROFILE?",
]:
    if token not in frontend:
        errors.append(f"frontend optimizer UX missing: {token}")

for token in [
    "OPTIMIZING CPU",
    "DO NOT POWER OFF",
    "KEEP CHARGER CONNECTED",
    "OPTIMIZATION WILL RESUME AUTOMATICALLY",
    "UNPLUG CHARGER TO CANCEL SAFELY",
    "read_progress(&completed, &total);",
    "while (running)\n        pause();",
]:
    if token not in progress:
        errors.append(f"progress splash missing: {token}")

# Splash is deliberately read-only after drawing.
for forbidden in [
    "fopen(CALIBRATION_FILE, \"w",
    "fopen(CALIBRATION_FILE, \"a",
    "sync()",
    "unlink(",
    "rename(",
]:
    if forbidden in progress:
        errors.append(f"progress splash must remain read-only: {forbidden}")

for token in [
    '"$ROOT/state/optimizer/ui-return"',
    '"$ROOT/bin/flip-optimizer-progress"',
    "OPTIMIZER_PROGRESS_PID",
    '"$ROOT/bin/flip-optimizer-calibration" publish',
    "OPTIMIZER_SESSION: completed profile published; previous snapshot discarded",
    "restoring previous snapshot",
    '"$ROOT/bin/flip-optimizer-calibration" restore-snapshot',
    '"$ROOT/bin/flip-optimizer-calibration" discard-snapshot',
    "optimizer_wait_external_power_online()",
    'POWER_WAIT=0',
    'while [ "$POWER_WAIT" -lt 5 ]',
    "charger became visible after boot settle",
]:
    if token not in updater:
        errors.append(f"updater optimizer UX hook missing: {token}")

start_pos = updater.find('"$ROOT/bin/flip-optimizer-progress" >/dev/null')
resume_pos = updater.find('"$ROOT/bin/flip-optimizer-session" boot-resume')
kill_pos = updater.find('kill "$OPTIMIZER_PROGRESS_PID"')
frontend_pos = updater.find('"$ROOT/bin/flip-frontend" >> "$LOG_TARGET"')

if min(start_pos, resume_pos, kill_pos, frontend_pos) < 0 or not (
    start_pos < resume_pos < kill_pos < frontend_pos
):
    errors.append(
        "optimizer progress lifecycle must be splash -> boot-resume -> kill -> frontend"
    )

for token in [
    'strcmp(argv[1], "publish")',
    'strcmp(argv[1], "reset")',
    "flip_optimizer_calibration_build_result(",
    "flip_optimizer_save(&result)",
    "guard.active",
    "guard.stock_only",
    'strcmp(argv[1], "snapshot")',
    'strcmp(argv[1], "restore-snapshot")',
    'strcmp(argv[1], "discard-snapshot")',
    'strcmp(argv[1], "reset-for-rerun")',
    "copy_file_durable(",
    "OPTIMIZER_HISTORY_FILE",
    "append_history_unstable(",
    "append_history_boundary(",
    "snapshot_preserved=1",
]:
    if token not in caltool:
        errors.append(f"calibration publish/reset safety missing: {token}")

snapshot_start = caltool.find("static int snapshot_optimizer_state")
snapshot_end = caltool.find("static int read_snapshot_marker(", snapshot_start + 1)
snapshot_block = caltool[snapshot_start:snapshot_end]
if snapshot_start < 0 or 'snapshot_preserved=1' not in snapshot_block:
    errors.append("unresolved rollback snapshot must be preserved across reruns")

reset_start = caltool.find("static int reset_calibration_state")
reset_end = caltool.find("static void usage", reset_start)
reset_block = caltool[reset_start:reset_end]
if "OPTIMIZER_HISTORY_FILE" in reset_block:
    errors.append("ordinary optimizer reset must not erase hard-failure history")

# A fresh optimization after explicit Reset Optimization has no calibration
# to snapshot. Snapshot creation must accept that empty baseline.
if 'if (have_calibration &&\n        (flip_optimizer_calibration_load(&calibration) < 0 ||' not in caltool:
    errors.append("fresh-start snapshot must allow no existing calibration")

init_start = caltool.find('if (strcmp(argv[1], "init") == 0)')
init_end = caltool.find('if (strcmp(argv[1], "status") == 0)', init_start)
init_block = caltool[init_start:init_end]
if "apply_failure_history(" in init_block:
    errors.append("historical evidence must not influence fresh calibration candidate selection")

power_gate = updater.find("# A UI-started optimization uses charger presence as a physical cancel switch.")
power_gate_end = updater.find("RUNTIME_ACTIVE_MARKER=", power_gate)
power_gate_block = updater[power_gate:power_gate_end]
if "! optimizer_wait_external_power_online" not in power_gate_block:
    errors.append("optimizer boot cancel must debounce transient charger absence")
if "! optimizer_external_power_online; then" in power_gate_block:
    errors.append("optimizer boot cancel must not use a single-shot power check")

# Runtime quarantine belongs to the validation/runtime safety domain. It may
# only be cleared when an optimizer session that existed at boot genuinely
# transitions to a newly completed profile; an unrelated normal/recovery boot
# must never unquarantine a profile that just caused a hard recovery.
clear_quarantine_pos = updater.find('rm -f "$ROOT/state/optimizer/runtime-profile-quarantined"')
session_gate_pos = updater.rfind('[ "$OPTIMIZER_SESSION_WAS_PRESENT" -eq 1 ]', 0, clear_quarantine_pos)
if clear_quarantine_pos < 0 or session_gate_pos < 0:
    errors.append("runtime quarantine clear must be gated by a pre-existing optimizer session")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer UI architecture passed")
