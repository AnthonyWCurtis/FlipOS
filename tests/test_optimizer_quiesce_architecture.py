#!/usr/bin/env python3

from pathlib import Path

candidate = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()
quiesce = Path("src/runtime/flip_optimizer_quiesce.c").read_text()
errors = []

required_quiesce = [
    "flip_optimizer_quiesce_cmdline_is_writer",
    "SIGSTOP",
    "SIGCONT",
    "sync();",
    "pid_is_stopped",
]
for token in required_quiesce:
    if token not in quiesce:
        errors.append(f"quiesce module missing: {token}")

live_start = candidate.find("static int run_live_once")
live_end = candidate.find("static int run_quiesce_probe", live_start)
live = candidate[live_start:live_end]

begin_pos = live.find("flip_optimizer_quiesce_begin(")
deadman_pos = live.find("arm_deadman(")
enter_pos = live.find("enter_candidate_frequency(")
parking_pos = live.find("return_to_parking_frequency(")
restore_pos = live.find("flip_cpu_policy_restore(")
disarm_pos = live.find("disarm_deadman(")
end_pos = live.find("flip_optimizer_quiesce_end(")
unstable_pos = live.find("flip_optimizer_calibration_session_confirm_unstable(")
stable_pos = live.find("flip_optimizer_calibration_session_confirm_stable(")

positions = [
    begin_pos,
    deadman_pos,
    enter_pos,
    parking_pos,
    restore_pos,
    disarm_pos,
    end_pos,
    unstable_pos,
    stable_pos,
]
if any(pos < 0 for pos in positions):
    errors.append("live candidate is missing one or more quiesce/recovery operations")
else:
    if not (begin_pos < deadman_pos < enter_pos):
        errors.append(
            "storage quiesce must begin before deadman arm and candidate voltage entry"
        )
    if not (parking_pos < restore_pos < disarm_pos < end_pos):
        errors.append(
            "writers must resume only after parking/table/policy restore and deadman disarm"
        )
    if not (end_pos < unstable_pos and end_pos < stable_pos):
        errors.append(
            "writers must resume before durable calibration PASS/FAIL commit"
        )

if "quiesce-probe" not in candidate or "QUIESCE_PROBE_OK" not in candidate:
    errors.append("candidate missing no-voltage quiesce probe")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer storage quiesce architecture passed")
