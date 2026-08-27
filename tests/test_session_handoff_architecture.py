#!/usr/bin/env python3

from pathlib import Path

session_h = Path("include/flip_session.h").read_text()
adapter_h = Path("include/flip_session_adapter.h").read_text()
adapter_c = Path("src/runtime/flip_session_adapter.c").read_text()
session_c = Path("src/runtime/flip_session.c").read_text()
frontend_c = Path("src/frontend/flip_frontend.c").read_text()
lifecycle_h = Path("include/flip_lifecycle.h").read_text()

required_session_tokens = [
    "FLIP_SESSION_EXIT_SAVED_HANDOFF",
    "flip_session_action_from_exit_code(",
]

for token in required_session_tokens:
    if token not in session_h and token not in session_c:
        print(f"missing generic handoff protocol token: {token}")
        raise SystemExit(1)

if "flip_session_action_from_exit_code(" not in session_c:
    print("session process layer must use generic exit-code mapping")
    raise SystemExit(1)

for path, text in [
    ("include/flip_session_adapter.h", adapter_h),
    ("src/runtime/flip_session_adapter.c", adapter_c),
]:
    if "map_exit_code" in text:
        print(f"adapter still owns exit-code mapping: {path}")
        raise SystemExit(1)

if "gb_map_exit_code" in adapter_c:
    print("GB/GBC adapter still carries per-core exit mapping")
    raise SystemExit(1)

for path, text in [
    ("src/runtime/flip_session_adapter.c", adapter_c),
    ("src/frontend/flip_frontend.c", frontend_c),
]:
    if "== 20" in text or "!= 20" in text:
        print(f"literal exit code 20 remains in {path}")
        raise SystemExit(1)


if (
    "FLIP_LIFECYCLE_EXIT_SWITCHER "
    "FLIP_SESSION_EXIT_SAVED_HANDOFF"
    not in lifecycle_h
):
    print(
        "lifecycle switcher exit code must alias "
        "FLIP_SESSION_EXIT_SAVED_HANDOFF"
    )
    raise SystemExit(1)

if "#define FLIP_LIFECYCLE_EXIT_SWITCHER 20" in lifecycle_h:
    print("lifecycle header still owns literal exit code 20")
    raise SystemExit(1)

if "WIFSIGNALED(status)" not in session_c:
    print("signaled child handling disappeared from session layer")
    raise SystemExit(1)

signaled_pos = session_c.index("WIFSIGNALED(status)")
signaled_tail = session_c[signaled_pos:signaled_pos + 1200]

if "FLIP_SESSION_ACTION_ERROR" not in signaled_tail:
    print("signaled child path must remain FLIP_SESSION_ACTION_ERROR")
    raise SystemExit(1)

print("session handoff architecture boundary passed")
