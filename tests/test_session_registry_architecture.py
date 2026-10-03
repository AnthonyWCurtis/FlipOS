from pathlib import Path

session = Path("src/runtime/flip_session.c").read_text()
makefile = Path("Makefile").read_text()

assert '#include "flip_session_registry.h"' in session
assert "flip_session_registry_upsert(" in session
assert "flip_session_registry_remove(" in session
assert 'strcmp(state, "exited") == 0' in session
assert 'strcmp(state, "closed") == 0' in session
assert "Registry failures are logged" in session
assert "$(BUILD)/flip_session_registry.o" in makefile
assert "test-session-registry" in makefile

print("session registry architecture test passed")
