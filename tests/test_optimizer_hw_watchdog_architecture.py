#!/usr/bin/env python3
from pathlib import Path

src = Path("src/runtime/flip_optimizer_hw_watchdog.c").read_text()
hdr = Path("include/flip_optimizer_hw_watchdog.h").read_text()
candidate = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()

errors = []

for token in [
    '"/dev/watchdog0"',
    "WDIOC_GETSUPPORT",
    "WDIOF_SETTIMEOUT",
    "WDIOF_KEEPALIVEPING",
    "WDIOC_SETTIMEOUT",
    "WDIOC_GETTIMEOUT",
    "WDIOC_KEEPALIVE",
    "watchdog->touched = 1;",
    "watchdog->armed = 1;",
]:
    if token not in src:
        errors.append(f"hardware watchdog missing required behavior: {token}")

if src.find("watchdog->touched = 1;") > src.find("WDIOC_GETSUPPORT"):
    errors.append("watchdog must become reboot-required immediately after open")

# On this Miyoo kernel/DT, close/magic-close cannot physically stop the WDT.
# The optimizer must never expose a normal disarm-and-continue API.
for forbidden in [
    "WDIOC_SETOPTIONS",
    "WDIOS_DISABLECARD",
    "WDIOF_MAGICCLOSE",
    "magic_close",
    "hw_watchdog_disarm",
]:
    if forbidden in src or forbidden in hdr:
        errors.append(f"non-stoppable watchdog must not expose disable path: {forbidden}")

if "caller must finish by rebooting" not in hdr:
    errors.append("header must document reboot-required ownership semantics")


live_start = candidate.find("static int run_live_once")
live_end = candidate.find("static int run_quiesce_probe", live_start)
live = candidate[live_start:live_end]

tsadc_arm = live.find("tsadc_recovery_set(1)")
hw_arm = live.find("flip_optimizer_hw_watchdog_arm(")
deadman_arm = live.find("arm_deadman(")
mark_entering = live.find("flip_optimizer_guard_mark_entering(")
enter_voltage = live.find("enter_candidate_frequency(")
hw_touched_branch = live.find("if (hw_watchdog.touched)")
tsadc_restore = live.find("tsadc_recovery_set(0)")

order = [
    tsadc_arm,
    hw_arm,
    deadman_arm,
    mark_entering,
    enter_voltage,
    hw_touched_branch,
    tsadc_restore,
]
if any(pos < 0 for pos in order):
    errors.append("candidate missing hardware-watchdog lifecycle operation")
elif order != sorted(order):
    errors.append(
        "candidate must arm TSADC, arm hardware watchdog, arm deadman, "
        "mark entering, enter voltage, handle touched watchdog, then "
        "allow normal TSADC restore only for pre-watchdog aborts"
    )

if "&hw_watchdog" not in live[live.find("run_stress_window("):live.find("run_stress_window(") + 240]:
    errors.append("stress window must receive hardware watchdog heartbeat state")

settle_start = candidate.find("static void settle_hw_watchdog_trial_and_reboot")
settle_end = candidate.find("static int ensure_candidate_ready", settle_start)
settle = candidate[settle_start:settle_end]
for token in [
    "flip_optimizer_calibration_session_cancel_unapplied",
    "flip_optimizer_calibration_session_confirm_unstable",
    "flip_optimizer_calibration_session_confirm_stable",
    "flip_optimizer_hw_watchdog_ping",
    "sync();",
    "WATCHDOG_SETTLED_REBOOT",
    "emergency_reboot();",
]:
    if token not in settle:
        errors.append(f"watchdog settlement missing: {token}")


probe_start = candidate.find("static int run_watchdog_reboot_probe")
probe_end = candidate.find("static int run_kernel_recovery_probe", probe_start)
probe = candidate[probe_start:probe_end]

for token in [
    "flip_optimizer_quiesce_begin",
    "tsadc_recovery_set(1)",
    "flip_optimizer_hw_watchdog_arm",
    "flip_optimizer_hw_watchdog_ping",
    "WATCHDOG_REBOOT_PROBE_ARMED",
    "voltage_written=0",
    "WATCHDOG_REBOOT_PROBE_REBOOTING",
    "emergency_reboot();",
]:
    if token not in probe:
        errors.append(f"watchdog reboot probe missing: {token}")

for forbidden in [
    "module_apply_table(",
    "enter_candidate_frequency(",
    "flip_optimizer_calibration_session_confirm_stable",
    "flip_optimizer_calibration_session_confirm_unstable",
    "read_cpu_rail_uv(",
]:
    if forbidden in probe:
        errors.append(f"stock watchdog reboot probe must be voltage-neutral: {forbidden}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer hardware watchdog architecture passed")
