from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

session = (ROOT / "src/runtime/flip_session.c").read_text()
persistence = (ROOT / "src/runtime/flip_session_persistence.c").read_text()
session_h = (ROOT / "include/flip_session.h").read_text()
makefile = (ROOT / "Makefile").read_text()

assert '#include "flip_session_persistence.h"' in session_h
assert "flip_session_persistence_write_current(" in session
assert "flip_session_persistence_read_current(" in session
assert "flip_session_persistence_quarantine_current(" in session

assert "FLIP_SESSION_FILE" not in session
assert "version=1\\n" not in session
assert "fopen(" not in session or "recent" in session.lower()

assert "FLIP_SESSION_FILE" in persistence
assert "fsync(" in persistence
assert "rename(" in persistence

assert "test-session-persistence" in makefile
assert "tests/test_session_persistence_architecture.py" in makefile
assert "flip_session_persistence.o" in makefile

print("session persistence architecture: PASS")
