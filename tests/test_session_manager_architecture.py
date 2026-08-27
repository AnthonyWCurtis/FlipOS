#!/usr/bin/env python3

from pathlib import Path

implementation = Path("src/runtime/flip_session_manager.c")

if not implementation.exists():
    print(f"missing Session Manager implementation: {implementation}")
    raise SystemExit(1)

text = implementation.read_text()

required_tokens = [
    "flip_system_find(",
    "flip_system_supports_path(",
    "flip_system_resume_available(",
]

for token in required_tokens:
    if token not in text:
        print(
            f"Session Manager must use generic system API {token!r}"
        )
        raise SystemExit(1)

forbidden_tokens = [
    "Gambatte",
    "flip_gb",
    ".sav",
    ".state",
    ".restore-pending",
    "FLIP_SAVE_ROOT",
    "FLIP_STATE_ROOT",
]

for token in forbidden_tokens:
    if token in text:
        print(
            f"forbidden persistence/system token "
            f"{token!r} found in {implementation}"
        )
        raise SystemExit(1)

print("session manager architecture boundary passed")
