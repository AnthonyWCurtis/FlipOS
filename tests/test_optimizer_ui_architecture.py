#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
frontend = (ROOT / "src/frontend/flip_frontend.c").read_text()
progress = (ROOT / "src/hwtest/flip_optimizer_progress.c").read_text()
validation_progress = (ROOT / "src/hwtest/flip_optimizer_validation_progress.c").read_text()
updater = (ROOT / "tools/device/updater-playtest").read_text()
caltool = (ROOT / "src/hwtest/flip_optimizer_calibration_tool.c").read_text()

errors = []

for token in [
    "SCREEN_SETTINGS",
    "SCREEN_OPTIMIZER",
    "CPU OPTIMIZATION",
    "OPTIMIZE AGAIN",
    "RESTORE DEFAULT SETTINGS",
    "optimizer_external_power_online()",
    "battery < 30",
    "OPTIMIZER_UI_RETURN_PATH",
    '"start",\n            "20"',
    '"snapshot"',
    '"reset-for-rerun"',
    '"restore-snapshot"',
    "RUN STABILITY TEST",
    "OPTIMIZER_VALIDATION_REQUEST_PATH",
    "OPTIMIZER_VALIDATION_RESULT_PATH",
    "optimizer_start_validation(unsigned duration_minutes)",
    "START STABILITY TEST?",
    "QUICK - 5 MIN",
    "STANDARD - 10 MIN",
    "THOROUGH - 20 MIN",
    "EXTENDED - 30 MIN",
    "FULL - 60 MIN",
]:
    if token not in frontend:
        errors.append(f"frontend optimizer UX missing: {token}")

for token in [
    "CPU STABILITY TEST",
    "STRESS TIME REMAINING",
    "TESTING %u MHz @ %u.%u mV",
    "DONE",
    "TEST",
    "WAIT",
    "C%d",
    "/proc/stat",
    "LOAD WINDOW %u/%u   %u/%u SEC",
    "CPU TEMP %u C   PEAK %u C",
    "RECOVERY PROTECTION ACTIVE",
    "KEEP CHARGER CONNECTED",
    "UNPLUG CHARGER TO CANCEL SAFELY",
    "read_cpu_samples",
    "read_validation_status",
    "cpu_usage_percent",
    "validation_frequencies_khz",
    "current_chunk_elapsed",
    "stress_elapsed_seconds",
]:
    if token not in validation_progress:
        errors.append(f"validation progress UI missing: {token}")

for token in [
    "OPTIMIZING CPU",
    "DO NOT POWER OFF",
    "KEEP CHARGER CONNECTED",
    "PROGRESS IS SAVED AUTOMATICALLY",
    "PERFORMANCE LEVEL %u OF %u",
    "VOLTAGE TEST%s COMPLETED",
    "EST. %u-%u MIN REMAINING",
    "RAM_PROGRESS_FILE",
    "read_ram_progress(&ram)",
    "opp_progress_permille",
    "estimate_trials",
    "STRESS TEST",
    "RECOVERY PROTECTION ACTIVE",
    "nanosleep(&delay, NULL)",
]:
    if token not in progress:
        errors.append(f"dynamic progress UI missing: {token}")

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

if "taskset -c 0,1,2 nice -n 10" not in updater:
    errors.append("optimizer progress must stay off recovery CPU3 and run low priority")

if 'nice -n 10 "$ROOT/bin/flip-optimizer-validation-progress"' not in updater:
    errors.append("validation progress UI must run at low scheduler priority")

for token in ["CALIBRATED", "VERIFIED", "DISABLED"]:
    if token not in frontend:
        errors.append(f"optimizer top-level state missing: {token}")

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
if 'mkdir(FLIP_OPTIMIZER_CALIBRATION_ROOT, 0755)' not in snapshot_block:
    errors.append("fresh-install snapshot must create optimizer state directory")

reset_start = caltool.find("static int reset_calibration_state")
reset_end = caltool.find("static void usage", reset_start)
reset_block = caltool[reset_start:reset_end]
if "OPTIMIZER_HISTORY_FILE" in reset_block:
    errors.append("ordinary optimizer reset must not erase hard-failure history")

# A fresh optimization after explicit Reset Optimization has no calibration
# to snapshot. Snapshot creation must accept that empty baseline.
if 'if (have_calibration &&\n        (flip_optimizer_calibration_load(&calibration) < 0 ||' not in caltool:
    errors.append("fresh-start snapshot must allow no existing calibration")

power_gate = updater.find("# A UI-started optimization uses charger presence as a physical cancel switch.")
power_gate_end = updater.find("RUNTIME_ACTIVE_MARKER=", power_gate)
power_gate_block = updater[power_gate:power_gate_end]
if "! optimizer_wait_external_power_online" not in power_gate_block:
    errors.append("optimizer boot cancel must debounce transient charger absence")
if "! optimizer_external_power_online; then" in power_gate_block:
    errors.append("optimizer boot cancel must not use a single-shot power check")
if '[ "$OPTIMIZER_BOOT_MODE" = "normal" ]' not in power_gate_block:
    errors.append("optimizer charger cancel must not discard crash-recovery evidence")

# Runtime quarantine belongs to the validation/runtime safety domain. It may
# only be cleared when an optimizer session that existed at boot genuinely
# transitions to a newly completed profile; an unrelated normal/recovery boot
# must never unquarantine a profile that just caused a hard recovery.
clear_quarantine_pos = updater.find('rm -f "$ROOT/state/optimizer/runtime-profile-quarantined"')
session_gate_pos = updater.rfind('[ "$OPTIMIZER_SESSION_WAS_PRESENT" -eq 1 ]', 0, clear_quarantine_pos)
if clear_quarantine_pos < 0 or session_gate_pos < 0:
    errors.append("runtime quarantine clear must be gated by a pre-existing optimizer session")

validation_active_pos = updater.find('[ -f "$VALIDATION_ACTIVE" ]')
runtime_stale_pos = updater.find('OPTIMIZER_RUNTIME: stale active marker quarantined')
if min(validation_active_pos, runtime_stale_pos) < 0 or validation_active_pos >= runtime_stale_pos:
    errors.append("boot must classify validation-active before generic runtime stale marker")

init_start = caltool.find('strcmp(argv[1], "init")')
init_end = caltool.find('strcmp(argv[1], "status")', init_start)
init_block = caltool[init_start:init_end]
if init_start < 0 or init_end < 0 or "apply_failure_history(&calibration)" not in init_block:
    errors.append("optimizer init must import persisted hard-failure history before saving")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

calibration_tool = (ROOT / "src/hwtest/flip_optimizer_calibration_tool.c").read_text()
for token in [
    'DT_SERIAL_PATH "/sys/firmware/devicetree/base/serial-number"',
    'fopen(DT_SERIAL_PATH, "rb")',
]:
    if token not in calibration_tool:
        errors.append(f"stable optimizer identity support missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer UI architecture passed")
