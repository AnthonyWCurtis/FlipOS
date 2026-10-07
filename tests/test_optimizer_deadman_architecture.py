#!/usr/bin/env python3

from pathlib import Path

source = Path("src/hwtest/flip_optimizer_deadman.c").read_text()
candidate = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()

errors = []

deadman_required = [
    '"probe"',
    "PROBE_OK",
    "mlockall(MCL_CURRENT | MCL_FUTURE)",
    "SCHED_FIFO",
    "sched_setscheduler",
    "CLOCK_MONOTONIC",
    "TIMER_ABSTIME",
    "SYS_reboot",
    "LINUX_REBOOT_CMD_RESTART2",
    '"watchdog"',
    "write(ready_fd, &ready, 1)",
]
for token in deadman_required:
    if token not in source:
        errors.append(f"deadman missing safety primitive: {token}")

for forbidden in ["system(", "popen(", "sync("]:
    if forbidden in source:
        errors.append(f"deadman must not depend on unsafe primitive: {forbidden}")

candidate_required = [
    "arm_deadman(",
    "disarm_deadman(",
    "FLIP_OPTIMIZER_DEADMAN_PATH",
    "deadman_pid",
    "deadman armed",
]
for token in candidate_required:
    if token not in candidate:
        errors.append(f"candidate missing deadman integration: {token}")

live_start = candidate.find("static int run_live_once")
live_end = candidate.find("static void usage", live_start)
live = candidate[live_start:live_end]
arm_pos = live.find("arm_deadman(")
enter_pos = live.find("enter_candidate_frequency(")
disarm_pos = live.find("disarm_deadman(")
confirm_pos = live.find("flip_optimizer_calibration_session_confirm_stable")

if min(arm_pos, enter_pos, disarm_pos, confirm_pos) < 0:
    pass
elif not (arm_pos < enter_pos < disarm_pos < confirm_pos):
    errors.append("deadman must arm before voltage entry and disarm before durable confirmation")

if "sync();" in candidate[candidate.find("static void emergency_reboot"):candidate.find("static int ensure_candidate_ready")]:
    errors.append("emergency reboot must not sync storage during unstable recovery")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer deadman architecture passed")
