from pathlib import Path

SOURCE = Path("src/emulators/flip_gb.c").read_text(encoding="utf-8")

FORBIDDEN = [
    "state_path_hash",
    "ensure_state_directory",
    "fopen(",
    "fsync(",
    "rename(",
    "GB_STATE_DIR",
]

REQUIRED = [
    "flip_state_context_t",
    "flip_state_save",
    "flip_state_restore",
]

for token in FORBIDDEN:
    assert token not in SOURCE, f"flip_gb.c still owns state storage token: {token}"

for token in REQUIRED:
    assert token in SOURCE, f"flip_gb.c missing State Manager integration token: {token}"

print("state manager architecture test passed")
