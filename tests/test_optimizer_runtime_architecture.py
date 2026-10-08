from pathlib import Path

source = Path("src/hwtest/flip_optimizer_runtime_tool.c").read_text()
errors=[]
for token in [
    '"/mnt/SDCARD/flipos-dev/lib/flip_opp_runtime_profile.ko"',
    '"/sys/module/flip_opp_runtime_profile/parameters/control"',
    'flip_optimizer_load(result)',
    'strcmp(device_id, result->device_id) != 0',
    'guard.active',
    'guard.stock_only',
    'access(SESSION_FILE, F_OK) == 0',
    'access(UI_RETURN_FILE, F_OK) == 0',
    'flip_cpu_policy_set_limits(PARK_KHZ, PARK_KHZ)',
    'stock.target_uv != opp->stock_uv',
    'write_text(RUNTIME_CONTROL, "restore-all")',
    'unload_module()',
    'flip_cpu_policy_restore(&snapshot)',
]:
    if token not in source:
        errors.append(f"runtime optimizer controller missing safety token: {token}")
apply = source.find("static int apply_profile")
park = source.find("park_cpu(&snapshot)", apply)
load = source.find("load_module()", park)
loop = source.find("for (size_t i = 0; i < result.opp_count; ++i)", load)
restore_policy = source.find("flip_cpu_policy_restore(&snapshot)", loop)
if min(apply, park, load, loop, restore_policy) < 0 or not (apply < park < load < loop < restore_policy):
    errors.append("runtime profile flow must validate -> park -> load -> apply -> restore CPU policy")
if errors:
    print("\n".join(errors)); raise SystemExit(1)
print("optimizer runtime controller architecture passed")
