from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/hwtest/flip_optimizer_runtime_tool.c").read_text()
errors = []

for token in [
    'apply_profile(',
    'strcmp(argv[1], "apply-validation")',
    'apply_profile(0, 0)',
    'runtime_marker_written=%d',
    'Keep cpuinfo only as a legacy',
    'fopen(DT_SERIAL_PATH, "rb")',
    'DT_SERIAL_PATH "/sys/firmware/devicetree/base/serial-number"',
    "wait_for_frequency(",
    "choose_alternate_frequency(",
    "restore_all_with_handoff(",
    "opp->frequency_khz == PARK_KHZ",
    "pin_cpu(handoff_khz)",
]:
    if token not in source:
        errors.append(f"runtime validation apply support missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)


# Normal gameplay may only use a profile that has passed validation. The
# validator itself uses apply-validation and is deliberately allowed to test
# the not-yet-verified profile.
for token in [
    "VALIDATION_RESULT_FILE",
    "validation_passed_for_result(",
    "require_validation",
    "apply_profile(1, 1)",
    "apply_profile(0, 0)",
    'strcmp(status, "passed") == 0',
    "calibrated_epoch == result->calibrated_epoch",
]:
    assert token in source, f"runtime validation gate missing: {token}"

print("optimizer runtime architecture passed")
