# Phase 7L Formal Session Persistence Implementation Plan

Date: 2026-10-04

## Task 1 — Current-session persistence service

- Add `flip_session_persistence.h` / `flip_session_persistence.c`.
- Move `flip_session_state_t` to the persistence interface.
- Preserve the exact version-1 current-session format.
- Implement atomic write, strict read and invalid-record quarantine.
- Add focused host tests.

## Task 2 — Session Manager integration

- Keep `flip_session_read_current()` as a compatibility wrapper.
- Route `write_session_state()` through persistence.
- Route invalid-current quarantine through persistence.
- Keep lifecycle decisions, recent history and registry mirroring in Session Manager.
- Do not alter `flip_session_registry`.

## Task 3 — Gates

Focused gate:

```text
make test-session-persistence
python3 tests/test_session_persistence_architecture.py
make frontend
```

Pre-hardware gate:

```text
make verify
sha256sum build/flip-frontend build/flip-gb
```

Hardware acceptance:

- preserve the Phase 7K accepted frontend as rollback;
- deploy the exact verified frontend;
- reboot and read the existing real `current` record through a persistence probe;
- confirm the frontend starts and existing interrupted-session classification survives unchanged;
- exercise a temporary persistence round-trip only in a separate test root, never against the real current record.

No physical lid/gameplay regression is required unless the extraction changes observable lifecycle behavior.
