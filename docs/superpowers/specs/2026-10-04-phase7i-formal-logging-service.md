# Phase 7I Formal Logging Service Design

Date: 2026-10-04

## Goal

Introduce a small shared logging service without changing the established hardware log text or creating a second competing persistent writer.

## Current state

FlipOS diagnostics are mostly direct `fprintf(stderr, ...)` calls. The launcher already captures stderr into the existing device log, so replacing that capture mechanism in this phase would add risk without improving ownership.

## Design

The runtime service exposes:

- `flip_log_write(level, component, format, ...)`
- configurable minimum severity
- configurable stream for host tests
- reset to the compatibility defaults
- canonical component log path construction beneath `FLIP_LOG_ROOT`

Default minimum severity is DEBUG so existing diagnostics are not silently removed.

Output intentionally remains:

```text
COMPONENT: message
```

Severity is policy metadata used for filtering; it is not inserted into the line format in Phase 7I.

## Storage ownership

Paths owns `FLIP_LOG_ROOT = FLIP_STORAGE_ROOT "/logs"` and `flip_paths_log_root()`.

Logging constructs canonical `<log-root>/<component>.log` paths. Component names are restricted to letters, digits, underscore and hyphen.

Phase 7I does not create directories, rotate files, or bypass the existing stderr capture.

## Initial migration

- Shared Lifecycle Host: all direct stderr logging moves through Logging.
- Frontend: the suspended-session boundary begins consuming Logging.
- Existing low-level diagnostics remain unchanged and can migrate incrementally.

## Compatibility requirements

- Existing lifecycle lines must remain text-compatible with hardware greps.
- Logging failures must not change lifecycle control flow.
- No new NAND/MTD or `/usr/miyoo` writes.
- Existing launcher log capture remains authoritative.

## Verification

Focused host tests cover level filtering, stream override, path validation and exact line format.

Architecture checks enforce Paths ownership, Lifecycle Host migration, frontend consumption and verify-target wiring.

Final gate is one full `make verify`, then real-device launch/suspend log compatibility.
