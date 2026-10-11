from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/hwtest/flip_optimizer_runtime_tool.c").read_text()
errors = []

for token in [
    'apply_profile(int write_marker)',
    'strcmp(argv[1], "apply-validation")',
    'apply_profile(0)',
    'runtime_marker_written=%d',
    'Keep cpuinfo only as a legacy',
    'fopen(DT_SERIAL_PATH, "rb")',
    'DT_SERIAL_PATH "/sys/firmware/devicetree/base/serial-number"',
]:
    if token not in source:
        errors.append(f"runtime validation apply support missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer runtime architecture passed")
