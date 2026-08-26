#!/usr/bin/env python3

from pathlib import Path

required_files = [
    Path("include/flip_session_adapter.h"),
    Path("src/runtime/flip_session_adapter.c"),
]

for path in required_files:
    if not path.exists():
        print(f"missing renamed session adapter file: {path}")
        raise SystemExit(1)

forbidden_files = [
    Path("include/flip_emulator_adapter.h"),
    Path("src/runtime/flip_emulator_adapter.c"),
]

for path in forbidden_files:
    if path.exists():
        print(f"legacy ambiguous adapter file remains: {path}")
        raise SystemExit(1)

paths = [
    Path("include/flip_system.h"),
    Path("src/runtime/flip_system.c"),
    Path("src/runtime/flip_session.c"),
    Path("Makefile"),
]

for path in paths:
    text = path.read_text()

    for token in [
        "flip_emulator_adapter",
        "->adapter",
        ".adapter",
    ]:
        if token in text:
            print(f"legacy adapter token {token!r} remains in {path}")
            raise SystemExit(1)

print("session adapter naming boundary passed")
