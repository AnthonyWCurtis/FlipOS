#!/usr/bin/env python3

from pathlib import Path

protocol_h = Path("include/flip_exit_protocol.h").read_text()
protocol_c = Path("src/runtime/flip_exit_protocol.c").read_text()
lifecycle_h = Path("include/flip_lifecycle.h").read_text()
lifecycle_c = Path("src/runtime/flip_lifecycle.c").read_text()
adapter_h = Path("include/flip_session_adapter.h").read_text()
adapter_c = Path("src/runtime/flip_session_adapter.c").read_text()
session_c = Path("src/runtime/flip_session.c").read_text()
makefile = Path("Makefile").read_text()

assert "#define FLIP_EXIT_CODE_NORMAL 0" in protocol_h
assert "#define FLIP_EXIT_CODE_SUSPEND 20" in protocol_h
assert "flip_exit_protocol_classify(" in protocol_c

assert '#include "flip_exit_protocol.h"' in lifecycle_h
assert "FLIP_EXIT_CODE_SUSPEND" in lifecycle_c
assert "FLIP_LIFECYCLE_EXIT_SWITCHER" not in lifecycle_h
assert "FLIP_LIFECYCLE_EXIT_SWITCHER" not in lifecycle_c

assert "map_exit_code" not in adapter_h
assert "map_exit_code" not in adapter_c
assert "exit_code == 20" not in adapter_c

assert '#include "flip_exit_protocol.h"' in session_c
assert "flip_exit_protocol_classify(" in session_c
assert "FLIP_EXIT_PROTOCOL_ERROR" in session_c
assert 'session_state =\n            "error";' in session_c

assert "$(BUILD)/flip_exit_protocol.o" in makefile
assert "test-exit-protocol" in makefile
assert "tests/test_exit_protocol_architecture.py" in makefile

print("exit protocol architecture test passed")
