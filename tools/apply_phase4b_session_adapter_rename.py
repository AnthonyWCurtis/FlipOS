#!/usr/bin/env python3

from pathlib import Path
import sys


def replace_exact(path: Path, old: str, new: str, expected: int, label: str) -> None:
    text = path.read_text()
    count = text.count(old)

    if count != expected:
        raise RuntimeError(
            f"{label}: expected {expected} matches in {path}, found {count}"
        )

    path.write_text(text.replace(old, new))


def main() -> int:
    old_header = Path("include/flip_emulator_adapter.h")
    new_header = Path("include/flip_session_adapter.h")
    old_source = Path("src/runtime/flip_emulator_adapter.c")
    new_source = Path("src/runtime/flip_session_adapter.c")

    if new_header.exists() and new_source.exists() and not old_header.exists() and not old_source.exists():
        print("Phase 4B session adapter rename already applied")
        return 0

    if not old_header.exists() or not old_source.exists():
        raise RuntimeError("legacy adapter files are not in the expected state")

    if new_header.exists() or new_source.exists():
        raise RuntimeError("renamed adapter files already exist unexpectedly")

    header = old_header.read_text()
    required_header_tokens = [
        "FLIP_EMULATOR_ADAPTER_H",
        "flip_emulator_adapter_t",
        "flip_emulator_adapter_for_system",
    ]

    for token in required_header_tokens:
        if token not in header:
            raise RuntimeError(f"header token missing before rename: {token}")

    header = header.replace("FLIP_EMULATOR_ADAPTER_H", "FLIP_SESSION_ADAPTER_H")
    header = header.replace("flip_emulator_adapter_t", "flip_session_adapter_t")
    header = header.replace(
        "flip_emulator_adapter_for_system",
        "flip_session_adapter_for_system",
    )
    new_header.write_text(header)

    source = old_source.read_text()
    required_source_tokens = [
        '#include "flip_emulator_adapter.h"',
        "flip_emulator_adapter_t",
        "flip_emulator_adapter_for_system",
        "gb_adapter",
    ]

    for token in required_source_tokens:
        if token not in source:
            raise RuntimeError(f"source token missing before rename: {token}")

    source = source.replace(
        '#include "flip_emulator_adapter.h"',
        '#include "flip_session_adapter.h"',
    )
    source = source.replace("flip_emulator_adapter_t", "flip_session_adapter_t")
    source = source.replace(
        "flip_emulator_adapter_for_system",
        "flip_session_adapter_for_system",
    )
    source = source.replace("gb_adapter", "gb_session_adapter")
    new_source.write_text(source)

    system_h = Path("include/flip_system.h")
    replace_exact(
        system_h,
        '#include "flip_emulator_adapter.h"',
        '#include "flip_session_adapter.h"',
        1,
        "system header include",
    )
    replace_exact(
        system_h,
        "const flip_emulator_adapter_t *adapter;",
        "const flip_session_adapter_t *session_adapter;",
        1,
        "system adapter field",
    )

    system_c = Path("src/runtime/flip_system.c")
    replace_exact(
        system_c,
        ".adapter = NULL",
        ".session_adapter = NULL",
        1,
        "system registry field",
    )
    replace_exact(
        system_c,
        "system->adapter",
        "system->session_adapter",
        5,
        "system adapter references",
    )
    replace_exact(
        system_c,
        "flip_emulator_adapter_for_system",
        "flip_session_adapter_for_system",
        1,
        "system adapter resolver",
    )

    session_c = Path("src/runtime/flip_session.c")
    replace_exact(
        session_c,
        '#include "flip_emulator_adapter.h"',
        '#include "flip_session_adapter.h"',
        1,
        "session include",
    )
    replace_exact(
        session_c,
        "const flip_emulator_adapter_t *adapter =",
        "const flip_session_adapter_t *session_adapter =",
        1,
        "session adapter local type",
    )
    replace_exact(
        session_c,
        "metadata->system->adapter",
        "metadata->system->session_adapter",
        1,
        "session system adapter field",
    )
    replace_exact(
        session_c,
        "if (adapter &&\n            adapter->map_exit_code)",
        "if (session_adapter &&\n            session_adapter->map_exit_code)",
        1,
        "session adapter guard",
    )
    replace_exact(
        session_c,
        "adapter->map_exit_code(",
        "session_adapter->map_exit_code(",
        1,
        "session adapter exit mapping",
    )

    makefile = Path("Makefile")
    replace_exact(
        makefile,
        "flip_emulator_adapter.o",
        "flip_session_adapter.o",
        2,
        "Makefile adapter object",
    )
    replace_exact(
        makefile,
        "src/runtime/flip_emulator_adapter.c",
        "src/runtime/flip_session_adapter.c",
        1,
        "Makefile adapter source",
    )

    old_header.unlink()
    old_source.unlink()

    scan_paths = [Path("include"), Path("src")]
    legacy_hits = []

    for root in scan_paths:
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in {".c", ".h"}:
                continue

            text = path.read_text()
            if "flip_emulator_adapter" in text:
                legacy_hits.append(str(path))

    if legacy_hits:
        raise RuntimeError(
            "legacy adapter naming remains in: " + ", ".join(legacy_hits)
        )

    print("Phase 4B session adapter rename applied")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"migration refused: {exc}", file=sys.stderr)
        raise SystemExit(1)
