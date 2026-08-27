from pathlib import Path

source = Path(
    "src/runtime/flip_session.c"
).read_text()

required = [
    "#ifndef FLIP_SESSION_STATE_ROOT",
    "#define FLIP_SESSION_STATE_ROOT",
]

for token in required:
    if token not in source:
        print(
            "session lifecycle storage is not "
            f"test-root configurable: {token}"
        )
        raise SystemExit(1)

# Production paths must derive from the configurable root.
for definition in [
    "SESSION_DIR",
    "SESSION_FILE",
]:
    pos = source.find(
        f"#define {definition}"
    )

    if pos < 0:
        print(
            f"session storage definition missing: {definition}"
        )
        raise SystemExit(1)

    window = source[
        pos:
        pos + 250
    ]

    if "FLIP_SESSION_STATE_ROOT" not in window:
        print(
            f"{definition} does not derive from "
            "FLIP_SESSION_STATE_ROOT"
        )
        raise SystemExit(1)

print("session storage architecture passed")
