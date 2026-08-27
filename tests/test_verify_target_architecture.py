#!/usr/bin/env python3

from pathlib import Path

MAKEFILE = Path("Makefile").read_text()

errors = []

required = [
    ".PHONY:",
    "verify",
    "test-state-recovery-health",
    "test-session-save-safety",
    "test-recovery",
    "test-architecture",
    "$(MAKE) all",
    "git diff --check",
    "/tmp/flipos-verify.log",
    'echo "VERIFY: PASS"',
    'echo "VERIFY: FAIL"',
]

for token in required:
    if token not in MAKEFILE:
        errors.append(f"compact verify target missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("compact verify target architecture passed")
