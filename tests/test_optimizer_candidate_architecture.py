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
    "tsadc_recovery_load()",
    "tsadc_recovery_set(1)",
    "tsadc_recovery_set(0)",
    "tsadc_recovery_unload()",
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
    "optimizer_ui_power_interlock_active()",
    "optimizer_external_power_online()",
    "CHARGER_REMOVED cancel=1",
    "CHARGER_CANCELLED",
    "settle_hw_watchdog_power_cancel_and_reboot(",
    "clear_ui_optimizer_session_files()",
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
live_quiesce = live.find("flip_optimizer_quiesce_begin(")
live_panic = live.find("flip_optimizer_panic_policy_begin(")
live_tsadc_arm = live.find("tsadc_recovery_set(1)")
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
live_deadman_disarm = live.find("disarm_deadman(&deadman_pid)")
live_tsadc_restore = live.find("tsadc_recovery_set(0)")
live_tsadc_unload = live.find("tsadc_recovery_unload()")
live_quiesce_end = live.find("flip_optimizer_quiesce_end(&quiesce)")
live_confirm = live.find(
    "flip_optimizer_calibration_session_confirm_stable"
)

positions = [
    live_ready,
    live_apply,
    live_quiesce,
    live_panic,
    live_tsadc_arm,
    live_deadman,
    live_mark_entering,
    live_enter,
    live_integrity_first,
    live_stress,
    live_integrity_second,
    live_park,
    live_restore,
    live_policy_restore,
    live_deadman_disarm,
    live_tsadc_restore,
    live_tsadc_unload,
    live_quiesce_end,
    live_confirm,
]

if min(positions) < 0:
    pass
elif positions != sorted(positions):
    errors.append(
        "live runner safety order must be guard, apply, quiesce, panic, TSADC, "
        "deadman, PMU entering, enter, integrity, stress, integrity, park, restore table, "
        "restore policy, disarm deadman, restore TSADC, resume writers, confirm"
    )


power_settle_start = source.find(
    "static void settle_hw_watchdog_power_cancel_and_reboot"
)
power_settle_end = source.find(
    "static int ensure_candidate_ready",
    power_settle_start,
)
power_settle = source[power_settle_start:power_settle_end]

power_cancel_pos = power_settle.find(
    "flip_optimizer_calibration_session_cancel_unapplied"
)
power_session_clear = power_settle.find(
    "clear_ui_optimizer_session_files()"
)
power_reboot = power_settle.rfind("emergency_reboot();")

if min(power_cancel_pos, power_session_clear, power_reboot) < 0:
    errors.append("charger-cancel settlement is incomplete")
elif not (power_cancel_pos < power_session_clear < power_reboot):
    errors.append(
        "charger-cancel must cancel trial, clear session, then reboot"
    )

if "confirm_stable" in power_settle or "confirm_unstable" in power_settle:
    errors.append(
        "charger cancellation must never classify voltage stable/unstable"
    )


if 'unlink(\n            FLIP_OPTIMIZER_UI_RETURN_FILE)' in source[source.find("static int clear_ui_optimizer_session_files"):source.find("static int make_opp_supply_path")]:
    errors.append("live charger cancel must preserve ui-return for boot rollback")

if "candidate_uv < FLIP_OPTIMIZER_STRESS_MAX_TEMP_MC" in source:
    errors.append("temperature and voltage limits are mixed up")

for token in [
    'strcmp(argv[1], "validate-profile")',
    'FLIP_OPTIMIZER_VALIDATION_ACTIVE',
    'FLIP_OPTIMIZER_VALIDATION_RESULT',
    'FLIP_OPTIMIZER_RUNTIME_TOOL',
    '"apply-validation"',
    'run_child_tool_with_watchdog(',
    'strcmp(argv[1], "raise-profile-voltage") == 0',
    'FLIP_OPTIMIZER_VALIDATION_RETRY_STEP_UV 25000U',
    'OPTIMIZER_VALIDATION: AUTO_RAISE',
    'validation-target-passed',
    'validation-adjustments.log',
    'recovery_armed=%d',
    'phase=%s',
    'validate-profile-frequency',
    'flip_optimizer_save(&adjusted)',
    'flip_optimizer_calibration_save(&calibration)',
    'opp->optimized_uv',
    'OPTIMIZER_VALIDATION: OPP_PASS',
    'OPTIMIZER_VALIDATION: COMPLETE',
]:
    if token not in source:
        errors.append(f"profile validation safety missing: {token}")

validation_start = source.find("static int run_validate_profile")
validation_end = source.find("static void usage", validation_start)
validation = source[validation_start:validation_end]
if validation_start < 0 or validation_end < 0:
    errors.append("profile validation implementation missing")
else:
    if "candidate_uv" in validation:
        errors.append("profile validator must not use experimental candidate voltages")
    if "flip_optimizer_calibration_session_prepare" in validation:
        errors.append("profile validator must not modify calibration search evidence")
    if "FLIP_OPTIMIZER_HW_WATCHDOG_REQUEST_SECONDS" not in validation:
        errors.append("profile validator must retain hardware watchdog recovery")
    if "optimizer_external_power_online()" not in validation:
        errors.append("profile validator must monitor external power")
    if "FLIP_OPTIMIZER_VALIDATION_PARK_KHZ" not in validation:
        errors.append("profile validator must use stock-preserved parking OPP")
    if "run_child_tool_with_watchdog(" not in validation:
        errors.append("profile validator child operations must service hardware watchdog")
    if "TRANSITION_PASS" not in validation or "chunks" not in validation:
        errors.append("profile validator must repeatedly transition through optimized OPPs")
    if "seconds_per_opp > FLIP_OPTIMIZER_STRESS_MAX_SECONDS" not in validation:
        errors.append("profile validator must support bounded extended stability durations")
    if validation.count("FLIP_OPTIMIZER_VALIDATION_PARK_KHZ") < 4:
        errors.append("profile validator must park at stock-safe OPP between validation points")

    park_before_apply = validation.find(
        "flip_cpu_policy_set_limits(\n            FLIP_OPTIMIZER_VALIDATION_PARK_KHZ"
    )
    apply_pos = validation.find('"apply-validation"')
    watchdog_pos = validation.find(
        "flip_optimizer_hw_watchdog_arm("
    )
    opp_begin_pos = validation.find(
        '"OPTIMIZER_VALIDATION: OPP_BEGIN'
    )
    restore_pos = validation.find(
        'run_child_tool_with_watchdog(\n            FLIP_OPTIMIZER_RUNTIME_TOOL,\n            "restore"'
    )
    policy_restore_pos = validation.find(
        "flip_cpu_policy_restore(&original)",
        restore_pos,
    )

    if min(park_before_apply, apply_pos, watchdog_pos, opp_begin_pos, restore_pos, policy_restore_pos) < 0:
        errors.append("profile validator parking/apply/restore sequence incomplete")
    elif not (
        park_before_apply < watchdog_pos < apply_pos < opp_begin_pos < restore_pos < policy_restore_pos
    ):
        errors.append(
            "profile validation must park -> arm recovery -> apply -> stress -> restore table -> restore policy"
        )

# Validation must not expose the optimized table before the full recovery
# envelope is armed, and it must use its own durable validation marker rather
# than the gameplay runtime-active marker.
validation = source[source.find("static int run_validate_profile"):source.find("static void usage")]
active_pos = validation.find("FLIP_OPTIMIZER_VALIDATION_ACTIVE")
watchdog_pos = validation.find("flip_optimizer_hw_watchdog_arm")
deadman_pos = validation.find("arm_deadman(")
apply_validation_pos = validation.find('"apply-validation"')
begin_pos = validation.find('"OPTIMIZER_VALIDATION: BEGIN')
if min(active_pos, watchdog_pos, deadman_pos, apply_validation_pos, begin_pos) < 0:
    errors.append("validation protected apply sequence missing")
elif not (active_pos < watchdog_pos < deadman_pos < apply_validation_pos < begin_pos):
    errors.append("validation must arm durable/recovery state before applying optimized table")
if 'run_child_tool(FLIP_OPTIMIZER_RUNTIME_TOOL, "apply")' in validation:
    errors.append("validation must not use gameplay runtime marker apply path")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer candidate runner architecture passed")
