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
    "RELEASE_BUILD_ARTIFACTS :=",
    "test-release-package: $(RELEASE_BUILD_ARTIFACTS)",
    "$(BUILD)/flip-charge-service",
    "$(BUILD)/flip-optimizer-runtime",
    "$(BUILD)/flip_opp_voltage.ko",
    "$(BUILD)/flip_opp_runtime_profile.ko",
    "$(BUILD)/flip_tsadc_recovery.ko",
    "$(BUILD)/flip_rk817_charger.ko",
]

for token in required:
    if token not in MAKEFILE:
        errors.append(f"compact verify target missing: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("compact verify target architecture passed")
