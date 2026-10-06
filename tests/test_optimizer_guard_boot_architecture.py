#!/usr/bin/env python3

from pathlib import Path

wrapper = Path("tools/device/updater-playtest").read_text()

errors = []

required = [
    'chmod 755 "$ROOT/bin/flip-optimizer-guard"',
    '"$ROOT/bin/flip-optimizer-guard" boot-check',
    'OPTIMIZER_GUARD: boot check failed',
    'if [ -x "$ROOT/bin/flip-playtest-logger.sh" ]',
    '"$ROOT/bin/flip-frontend" >> "$LOG" 2>&1',
]

for token in required:
    if token not in wrapper:
        errors.append(f"optimizer guard boot wrapper missing: {token}")

guard_pos = wrapper.find('"$ROOT/bin/flip-optimizer-guard" boot-check')
logger_pos = wrapper.find('if [ -x "$ROOT/bin/flip-playtest-logger.sh" ]')
frontend_pos = wrapper.find('"$ROOT/bin/flip-frontend" >> "$LOG" 2>&1')

if guard_pos < 0 or logger_pos < 0 or frontend_pos < 0:
    pass
elif not (guard_pos < logger_pos < frontend_pos):
    errors.append(
        "optimizer guard must run before playtest logger and frontend"
    )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("optimizer guard boot architecture passed")
