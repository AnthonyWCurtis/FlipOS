#!/usr/bin/env python3
"""Guard the clean-checkout and release-packaging verification contracts."""

from pathlib import Path
import re

makefile = Path("Makefile").read_text()
errors = []


def recipe(name: str) -> str:
    match = re.search(
        rf"^{re.escape(name)}:\s*\n((?:\t.*\n|\n|#.*\n)*)",
        makefile,
        re.MULTILINE,
    )
    if not match:
        errors.append(f"missing {name} recipe")
        return ""
    return match.group(1)


host_targets = re.search(
    r"^HOST_TEST_TARGETS\s*:=\s*\\\n((?:\t.*\n)+)",
    makefile,
    re.MULTILINE,
)
if not host_targets:
    errors.append("missing shared HOST_TEST_TARGETS list")
else:
    listed = re.findall(r"\btest-[a-z][a-z0-9-]*\b", host_targets.group(1))
    if len(listed) != 55 or len(listed) != len(set(listed)):
        errors.append("HOST_TEST_TARGETS must list all 55 unique host checks")
    for required in (
        "test-controls",
        "test-save",
        "test-state-recovery-health",
        "test-session-save-safety",
        "test-recovery",
        "test-optimizer-calibration",
        "test-gpu-client",
        "test-update-installer",
        "test-update-fetch",
        "test-release-module-inputs",
    ):
        if required not in listed:
            errors.append(f"HOST_TEST_TARGETS missing {required}")

host = recipe("test-host")
verify = recipe("verify")
package = recipe("verify-package")

if "$(MAKE) $(HOST_TEST_TARGETS)" not in host:
    errors.append("test-host must execute the shared host regression list")

for required in (
    "$(MAKE) test-host",
    "$(MAKE) test-playtest-deploy",
    "$(MAKE) test-architecture",
    "$(MAKE) gpu-service",
    "$(MAKE) all",
    "git diff --check",
    "/tmp/flipos-verify.log",
    'echo "VERIFY: PASS"',
    'echo "VERIFY: FAIL"',
):
    if required not in verify:
        errors.append(f"verify missing {required}")

if "test-release-package" in verify or "verify-package" in verify:
    errors.append("clean-checkout verify must not require release kernel modules")

for required in (
    "flip_opp_voltage.ko",
    "flip_opp_runtime_profile.ko",
    "flip_tsadc_recovery.ko",
    "flip_rk817_charger.ko",
    "$(MAKE) test-release-package",
):
    if required not in package:
        errors.append(f"verify-package missing {required}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("clean-checkout verification architecture passed")
