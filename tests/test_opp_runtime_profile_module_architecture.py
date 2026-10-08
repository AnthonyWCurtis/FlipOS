from pathlib import Path

source = Path("tools/kernel/flip_opp_runtime_profile/flip_opp_runtime_profile.c").read_text()
errors=[]
for token in [
    "#define FLIP_RUNTIME_MAX_OPPS 16",
    "#define FLIP_OPP_SAFE_MIN_UV 812500UL",
    "dev_pm_opp_adjust_voltage(",
    "optimized_uv < FLIP_OPP_SAFE_MIN_UV",
    "optimized_uv >= stock_target_uv",
    "current_target_uv != stock_target_uv",
    "flip_cpu_is_at(freq_khz)",
    "flip_restore_all_locked()",
    "restore-all",
    "module_param_cb(control, &flip_control_ops, NULL, 0600)",
    "flip_adjustments[FLIP_RUNTIME_MAX_OPPS]",
]:
    if token not in source:
        errors.append(f"runtime OPP profile module missing safety token: {token}")
for forbidden in ["i2c_smbus", "i2c_transfer", "regmap_write", "/dev/i2c", "RK860"]:
    if forbidden in source:
        errors.append(f"runtime profile module must not bypass kernel OPP API: {forbidden}")
apply=source.find("static int flip_apply_one_locked")
adjust=source.find("dev_pm_opp_adjust_voltage(", apply)
verify=source.find("verify_uv != optimized_uv", adjust)
active=source.find("flip_adjustments[slot].active = true", verify)
if min(apply,adjust,verify,active) < 0 or not (apply < adjust < verify < active):
    errors.append("runtime profile must adjust, verify, then mark OPP active")
exitp=source.find("static void __exit flip_runtime_profile_exit")
restore=source.find("flip_restore_all_locked()", exitp)
if exitp < 0 or restore < 0:
    errors.append("runtime profile unload must attempt full stock restore")
if errors:
    print("\n".join(errors)); raise SystemExit(1)
print("runtime OPP profile module architecture passed")
