# Phase 7I Formal Logging Service Implementation Plan

Date: 2026-10-04

## Task 1 — Logging API and canonical path

- Add `FLIP_LOG_ROOT` and `flip_paths_log_root()`.
- Add `flip_log.h` / `flip_log.c`.
- Preserve `COMPONENT: message` output.
- Default to DEBUG threshold for compatibility.
- Add host tests for formatting, filtering, invalid inputs and canonical paths.

## Task 2 — Integration

- Route Lifecycle Host logging through `flip_log_write()`.
- Begin frontend adoption at suspended-session discovery.
- Link Logging into both frontend and GB binaries.
- Add architecture guard.

## Task 3 — Verification and hardware

Focused development gate:

```text
make test-log test-lifecycle
python3 tests/test_logging_architecture.py
make frontend gb
```

Pre-hardware gate:

```text
make verify
sha256sum build/flip-frontend build/flip-gb
```

Hardware acceptance:

- deploy with rollback copies;
- reboot;
- verify frontend starts;
- resume Pokemon Red;
- synthetic physical-MENU equivalent returns to GameSwitcher;
- verify lifecycle log strings remain unchanged;
- verify save/state commit remains successful.

No physical lid cycle is required unless the automated regression shows a lifecycle discrepancy, because Phase 7I changes logging calls rather than lifecycle decisions.
