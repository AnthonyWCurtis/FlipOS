#!/usr/bin/env python3

from pathlib import Path

source = Path("src/hwtest/flip_optimizer_candidate_tool.c").read_text()

errors = []

required = [
    'strcmp(argv[1], "table-only") == 0',
    "flip_optimizer_calibration_session_ready_for_apply",
    "module_apply_table(",
    "module_restore_table()",
    "flip_optimizer_calibration_session_cancel_unapplied",
    "emergency_reboot()",
    "flip_cpu_policy_restore(&original)",
    "voltage_entered=0",
]

for token in required:
    if token not in source:
        errors.append(f"candidate runner missing safety token: {token}")

main_pos = source.find("int main(")
if main_pos < 0:
    errors.append("candidate runner main missing")
else:
    main = source[main_pos:]
    forbidden_cli = [
        '"apply"',
        '"run"',
        '"undervolt"',
        '"stress"',
    ]
    for token in forbidden_cli:
        if token in main:
            errors.append(
                f"candidate runner exposes premature voltage mode: {token}"
            )

runner_start = source.find("static int run_table_only")
runner_end = source.find("static void usage", runner_start)
runner = source[runner_start:runner_end]

ready_pos = runner.find(
    "flip_optimizer_calibration_session_ready_for_apply"
)
apply_pos = runner.find("module_apply_table(")
restore_pos = runner.find("module_restore_table()")
cancel_pos = runner.find(
    "flip_optimizer_calibration_session_cancel_unapplied"
)

if min(ready_pos, apply_pos, restore_pos, cancel_pos) < 0:
    pass
elif not (ready_pos < apply_pos < restore_pos < cancel_pos):
    errors.append(
        "candidate runner must gate, adjust table, restore table, then cancel"
    )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer candidate runner architecture passed")
