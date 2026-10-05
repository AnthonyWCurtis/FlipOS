#!/usr/bin/env python3

from pathlib import Path

RUNNER = Path("src/runtime/flip_emulator_runner.c").read_text()
ADAPTER = Path("src/runtime/flip_session_adapter.c").read_text()

errors = []

required_runner = [
    '#include "flip_recovery.h"',
    "flip_recovery_context_init(",
    "flip_recovery_resume_blocked(",
    "flip_recovery_clear_resume_block(",
    ".persistence_complete =",
]

for token in required_runner:
    if token not in RUNNER:
        errors.append(f"shared runner recovery integration missing: {token}")

forbidden_runner = [
    ".restore-pending",
    "FLIP_SESSION_RECOVERY_ROOT",
    "fopen(",
    "rename(",
    "unlink(",
]

for token in forbidden_runner:
    if token in RUNNER:
        errors.append(f"shared runner must not own recovery storage detail: {token}")

required_adapter = [
    '#include "flip_recovery.h"',
    "flip_recovery_context_init(",
    "flip_recovery_resume_blocked(",
]

for token in required_adapter:
    if token not in ADAPTER:
        errors.append(f"session adapter recovery policy missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("recovery integration architecture passed")
