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
    "test-session-manager",
    "test-session-manager-recovery",
    "test-gameswitcher-ui",
    "test-architecture",
    "$(BUILD)/flip_session_manager.o",
    "$(BUILD)/flip_gameswitcher_ui.o",
    "$(MAKE) all",
    "git diff --check",
    "/tmp/flipos-verify.log",
    'echo "VERIFY: PASS"',
    'echo "VERIFY: FAIL"',
]

for token in required:
    if token not in MAKEFILE:
        errors.append(f"compact verify target missing: {token}")


verify_start = MAKEFILE.find("verify:")

if verify_start < 0:
    errors.append("compact verify target missing: verify:")
else:
    verify_end = MAKEFILE.find(
        "\ntest-controls:",
        verify_start
    )

    if verify_end < 0:
        verify_end = len(MAKEFILE)

    verify_recipe = MAKEFILE[
        verify_start:
        verify_end
    ]

    for target in [
        "$(MAKE) test-session-manager",
        "$(MAKE) test-session-manager-recovery",
        "$(MAKE) test-gameswitcher-ui",
        "$(MAKE) test-architecture",
        "$(MAKE) all",
    ]:
        if target not in verify_recipe:
            errors.append(
                f"verify recipe missing Phase 6 gate: {target}"
            )

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("compact verify target architecture passed")
