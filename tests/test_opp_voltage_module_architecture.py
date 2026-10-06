#!/usr/bin/env python3

from pathlib import Path

source = Path(
    "tools/kernel/flip_opp_voltage/flip_opp_voltage.c"
).read_text()

errors = []

required = [
    "#define FLIP_OPP_STEP_UV 12500UL",
    "#define FLIP_OPP_SAFE_MIN_UV 812500UL",
    "dev_pm_opp_adjust_voltage(",
    "candidate_uv < FLIP_OPP_SAFE_MIN_UV",
    "candidate_uv >= stock_target_uv",
    "flip_cpu_is_at(freq_khz)",
    "if (at_target)",
    "return -EBUSY;",
    "current_target_uv != stock_target_uv",
    "flip_restore_locked()",
    "module_param_cb(control, &flip_control_ops, NULL, 0600)",
    "if (flip_adjustment.active)",
    "flip_restore_locked();",
]

for token in required:
    if token not in source:
        errors.append(f"OPP voltage module missing safety token: {token}")

for forbidden in [
    "i2c_smbus",
    "i2c_transfer",
    "regmap_write",
    "/dev/i2c",
    "RK860",
]:
    if forbidden in source:
        errors.append(
            f"OPP voltage module must not bypass kernel OPP API: {forbidden}"
        )

apply_pos = source.find("static int flip_apply_locked")
adjust_pos = source.find("dev_pm_opp_adjust_voltage(", apply_pos)
verify_pos = source.find("verify_uv != candidate_uv", adjust_pos)
active_pos = source.find("flip_adjustment.active = true", verify_pos)

if min(apply_pos, adjust_pos, verify_pos, active_pos) < 0:
    pass
elif not (apply_pos < adjust_pos < verify_pos < active_pos):
    errors.append(
        "module must adjust, verify candidate, then mark adjustment active"
    )

exit_pos = source.find("static void __exit flip_opp_voltage_exit")
restore_pos = source.find("flip_restore_locked();", exit_pos)

if exit_pos < 0 or restore_pos < 0 or restore_pos < exit_pos:
    errors.append("module unload must restore an active OPP adjustment")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("OPP voltage module architecture passed")
