from pathlib import Path

session = Path("src/runtime/flip_session.c").read_text()
registry = Path("src/runtime/flip_session_registry.c").read_text()
frontend = Path("src/frontend/flip_frontend.c").read_text()
makefile = Path("Makefile").read_text()

assert '#include "flip_session_registry.h"' in session
assert "flip_session_registry_upsert(" in session
assert "flip_session_registry_remove(" in session
assert "flip_session_close_registered(" in session
assert "flip_session_registry_find_state(" in registry
assert '#include "flip_session_registry.h"' in frontend
assert 'flip_session_registry_find_state(\n            "suspended"' in frontend
assert "recent_session_registry_state(" in frontend
assert "recent_session_is_recoverable(" in frontend
assert "flip_session_reconcile_registry(" in session
assert "flip_session_reconcile_registry()" in frontend
assert '"interrupted"' in session
assert '"SESSION_REGISTRY: pruned state=%s system=%s rom=%s\\n"' in session
assert '"SESSION_REGISTRY: recovered interrupted system=%s rom=%s\\n"' in session
assert '"RESTORE"' in frontend
assert "gameswitcher_confirm_system" in frontend
assert "flip_session_close_registered(" in frontend
assert "flip_session_remove_recent(\n                        target_system" in frontend

suspended_start = frontend.index("static int load_suspended_session(void)")
suspended_end = frontend.index("static int recent_index_for_path", suspended_start)
suspended_body = frontend[suspended_start:suspended_end]
assert "flip_session_read_current(" not in suspended_body
assert 'strcmp(state, "exited") == 0' in session
assert 'strcmp(state, "closed") == 0' in session
assert "Registry failures are logged" in session
assert "$(BUILD)/flip_session_registry.o" in makefile
assert "test-session-registry" in makefile

print("session registry architecture test passed")
