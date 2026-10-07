#!/usr/bin/env python3
from pathlib import Path

src = Path("src/hwtest/flip_optimizer_session_tool.c").read_text()
wrapper = Path("tools/device/updater-playtest").read_text()

errors = []

for token in [
    '"/auto-session"',
    '"start"',
    '"start-one"',
    '"stop"',
    '"status"',
    '"boot-resume"',
    "flip_optimizer_supervisor_runtime_stock_safe()",
    "flip_optimizer_calibration_is_complete(",
    "target_complete_count",
    "completed >= state.target_complete_count",
    "calibration.pending",
    'execl(',
    '"live-once"',
    '"candidate-exec"',
    '"pending-after-recovery"',
    '"runtime-stock-safe"',
]:
    if token not in src:
        errors.append(f"optimizer session controller missing: {token}")

# The boot wrapper may resume a session, but must never create one implicitly.
if "flip-optimizer-session start" in wrapper:
    errors.append("boot wrapper must never implicitly start optimizer sessions")

resume_pos = wrapper.find('"$ROOT/bin/flip-optimizer-session" boot-resume')
post_pos = wrapper.find("post-settlement enforcement")
frontend_pos = wrapper.find('"$ROOT/bin/flip-frontend" >> "$LOG_TARGET" 2>&1')
if min(resume_pos, post_pos, frontend_pos) < 0 or not (
    post_pos < resume_pos < frontend_pos
):
    errors.append("optimizer session resume must run after recovery and before frontend")

# boot-resume should not call calibration prepare directly; the candidate owns
# preparation and the dangerous-window safety machinery.
boot_start = src.find("static int boot_resume")
boot_end = src.find("static void usage", boot_start)
boot = src[boot_start:boot_end]
for forbidden in [
    "flip_optimizer_calibration_prepare_next(",
    "flip_optimizer_calibration_session_prepare(",
    "module_apply_table(",
    "enter_candidate_frequency(",
]:
    if forbidden in boot:
        errors.append(f"boot session controller must not own candidate setup: {forbidden}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer session architecture passed")
