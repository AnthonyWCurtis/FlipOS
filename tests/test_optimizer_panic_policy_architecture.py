#!/usr/bin/env python3

from pathlib import Path

candidate = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()
policy = Path("src/runtime/flip_optimizer_panic_policy.c").read_text()
errors = []

if "kernel-recovery-crash-test" not in candidate or "KERNEL_RECOVERY_CRASH_TEST_ARMED" not in candidate:
    errors.append("candidate missing controlled kernel crash acceptance command")

if "kernel-recovery-probe" not in candidate or "KERNEL_RECOVERY_PROBE_OK" not in candidate:
    errors.append("candidate missing no-crash kernel recovery probe")

for token in [
    "panic_on_oops",
    "panic_on_warn",
    "panic_on_rcu_stall",
    "PANIC_SECONDS_DURING_TEST 10",
    "flip_optimizer_panic_policy_begin",
    "flip_optimizer_panic_policy_end",
]:
    if token not in policy:
        errors.append(f"kernel recovery policy missing: {token}")

live_start = candidate.find("static int run_live_once")
live_end = candidate.find("static int run_quiesce_probe", live_start)
live = candidate[live_start:live_end]

quiesce_begin = live.find("flip_optimizer_quiesce_begin(")
policy_begin = live.find("flip_optimizer_panic_policy_begin(")
deadman = live.find("arm_deadman(")
mark_entering = live.find("flip_optimizer_guard_mark_entering(")
enter = live.find("enter_candidate_frequency(")
cpu_restore = live.find("flip_cpu_policy_restore(&original)")
policy_end = live.find("flip_optimizer_panic_policy_end(")
deadman_end = live.find("disarm_deadman(")
quiesce_end = live.find("flip_optimizer_quiesce_end(")

positions = [
    quiesce_begin,
    policy_begin,
    deadman,
    mark_entering,
    enter,
    cpu_restore,
    policy_end,
    deadman_end,
    quiesce_end,
]

if any(pos < 0 for pos in positions):
    errors.append("candidate missing one or more kernel recovery operations")
else:
    if not (
        quiesce_begin
        < policy_begin
        < deadman
        < mark_entering
        < enter
    ):
        errors.append(
            "candidate must quiesce, arm kernel recovery, arm deadman, "
            "mark PMU entering, then enter voltage"
        )

    if not (
        cpu_restore
        < policy_end
        < deadman_end
        < quiesce_end
    ):
        errors.append(
            "candidate must restore CPU, restore kernel policy, disarm "
            "deadman, then resume writers"
        )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer kernel recovery policy architecture passed")
