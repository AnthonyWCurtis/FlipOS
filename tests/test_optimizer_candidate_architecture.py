#!/usr/bin/env python3

from pathlib import Path

source = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()

errors = []

required = [
    'strcmp(argv[1], "integrity-only") == 0',
    'strcmp(argv[1], "table-only") == 0',
    'strcmp(argv[1], "live-once") == 0',
    "flip_optimizer_calibration_session_ready_for_apply",
    "flip_optimizer_supervisor_runtime_stock_safe",
    "flip_optimizer_guard_mark_entering",
    "PMU_ENTERING",
    "runtime stock-safe latch active",
    "module_apply_table(",
    "module_restore_table()",
    "flip_optimizer_calibration_session_cancel_unapplied",
    "flip_optimizer_calibration_session_confirm_stable",
    "flip_optimizer_calibration_session_confirm_unstable",
    "candidate_exercised",
    "LIVE_UNSTABLE",
    "enter_candidate_frequency(",
    "run_integrity_check(",
    "LIVE_INTEGRITY_OK phase=pre",
    "LIVE_INTEGRITY_OK phase=post",
    "run_stress_window(",
    "return_to_parking_frequency(",
    "read_cpu_rail_uv(",
    "read_soc_temp_mc(",
    "FLIP_OPTIMIZER_STRESS_MAX_TEMP_MC",
    "emergency_reboot()",
    "flip_cpu_policy_restore(&original)",
    "voltage_entered=0",
]

for token in required:
    if token not in source:
        errors.append(f"candidate runner missing safety token: {token}")

table_start = source.find("static int run_table_only")
table_end = source.find("static int run_live_once", table_start)
table_runner = source[table_start:table_end]

ready_pos = table_runner.find(
    "flip_optimizer_calibration_session_ready_for_apply"
)
apply_pos = table_runner.find("module_apply_table(")
restore_pos = table_runner.find("module_restore_table()")
cancel_pos = table_runner.find(
    "flip_optimizer_calibration_session_cancel_unapplied"
)

if min(ready_pos, apply_pos, restore_pos, cancel_pos) < 0:
    pass
elif not (ready_pos < apply_pos < restore_pos < cancel_pos):
    errors.append(
        "table-only runner must gate, adjust table, restore table, then cancel"
    )

live_start = source.find("static int run_live_once")
live_end = source.find("static void usage", live_start)
live = source[live_start:live_end]

live_ready = live.find(
    "flip_optimizer_calibration_session_ready_for_apply"
)
live_apply = live.find("module_apply_table(")
live_deadman = live.find("arm_deadman(")
live_mark_entering = live.find("flip_optimizer_guard_mark_entering(")
live_enter = live.find("enter_candidate_frequency(")
live_integrity_first = live.find("run_integrity_check(")
live_stress = live.find("run_stress_window(")
live_integrity_second = live.find(
    "run_integrity_check(",
    live_stress + 1,
)
live_park = live.find("return_to_parking_frequency(")
live_restore = live.find("module_restore_table()")
live_policy_restore = live.find("flip_cpu_policy_restore(&original)")
live_confirm = live.find(
    "flip_optimizer_calibration_session_confirm_stable"
)

positions = [
    live_ready,
    live_apply,
    live_deadman,
    live_mark_entering,
    live_enter,
    live_integrity_first,
    live_stress,
    live_integrity_second,
    live_park,
    live_restore,
    live_policy_restore,
    live_confirm,
]

if min(positions) < 0:
    pass
elif positions != sorted(positions):
    errors.append(
        "live runner safety order must be guard, apply, deadman, PMU entering, "
        "enter, integrity, stress, integrity, park, restore table, restore policy, confirm"
    )

if "candidate_uv < FLIP_OPTIMIZER_STRESS_MAX_TEMP_MC" in source:
    errors.append("temperature and voltage limits are mixed up")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer candidate runner architecture passed")
