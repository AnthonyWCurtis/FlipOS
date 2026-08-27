#!/usr/bin/env python3

from pathlib import Path

FRONTEND = Path("src/frontend/flip_frontend.c").read_text()
HEADER = Path("include/flip_session_adapter.h").read_text()
ADAPTER = Path("src/runtime/flip_session_adapter.c").read_text()

errors = []

required_header = [
    "supports_save_safety",
    "build_start_normal_argv",
    "build_save_safety_argv",
]
for token in required_header:
    if token not in HEADER:
        errors.append(f"session adapter Save Safety capability missing: {token}")

required_adapter = [
    '"--no-resume"',
    '"--save-safety-status"',
    '"--restore-previous-save"',
    '"--undo-last-restore"',
]
for token in required_adapter:
    if token not in ADAPTER:
        errors.append(f"session adapter command mapping missing: {token}")

required_frontend = [
    "RESUME GAME",
    "START NORMALLY",
    "SAVE SAFETY",
    "RESTORE PREVIOUS SAVE",
    "UNDO LAST RESTORE",
    "build_start_normal_argv",
    "build_save_safety_argv",
    "FLIP_SAVE_SAFETY:",
]
for token in required_frontend:
    if token not in FRONTEND:
        errors.append(f"generic frontend Save Safety UX missing: {token}")

forbidden_frontend = [
    ".sav",
    ".state",
    ".recovery",
    ".restore-pending",
    "FLIP_SAVE_ROOT",
    "FLIP_STATE_ROOT",
    "FLIP_SESSION_RECOVERY_ROOT",
    "792f9628ba43b7d9",
]
for token in forbidden_frontend:
    if token in FRONTEND:
        errors.append(f"frontend must not own persistence storage detail: {token}")

if errors:
    for error in errors:
        print(error)
    raise SystemExit(1)

print("frontend Save Safety architecture passed")
