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
    "OPTIMIZER_SESSION: completed profile published",
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
]:
    if token not in caltool:
        errors.append(f"calibration publish/reset safety missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer UI architecture passed")
