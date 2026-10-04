# Phase 7L Formal Session Persistence Service Design

Date: 2026-10-04

## Goal

Give the legacy current-session record a formal persistence owner without redesigning the already hardware-tested GameSwitcher/session lifecycle.

## Existing architecture

Phase 6 already provides:

- a version-1 `state/session/current` lifecycle record;
- a separate atomic multi-session registry;
- recent-history persistence;
- stale-running-session recovery;
- interrupted/suspended/error lifecycle policy.

Before Phase 7L, the version-1 current-record serializer/parser/quarantine code lived directly inside `flip_session.c`, mixing disk-format ownership with process/lifecycle management.

## Service boundary

`flip_session_persistence` now owns only the current-session record:

- atomic version-1 write;
- strict version-1 read/validation;
- malformed-record quarantine to `current.invalid.<epoch>`.

The existing `flip_session_registry` remains the owner of the multi-session registry.

Session Manager retains:

- launch/wait/process ownership;
- lifecycle state decisions;
- registry mirroring;
- recent history;
- stale-process validation and repair policy;
- user-facing close/reconcile behavior.

## Compatibility

The on-disk current-session format is unchanged:

```text
version=1
state=...
system=...
rom=...
program=...
pid=...
started=...
ended=...
exit_code=...
signal=...
```

Existing public `flip_session_read_current()` remains available as the Session Manager compatibility API and delegates to persistence.

Existing log labels such as `SESSION_STATE`, `SESSION_READ`, and `SESSION_RECOVER` remain at the Session Manager boundary.

## Durability

Current-record writes use:

1. staged `.tmp` file;
2. complete formatted write;
3. `fflush()`;
4. file-data `fsync()`;
5. close;
6. rename to `current`.

As elsewhere on the FAT32/vfat SD card, this is file-level transactional protection and does not claim journaled filesystem durability.

## Safety

- no emulator save/state data is moved or deleted;
- quarantine preserves malformed metadata for diagnostics;
- registry format and ownership are unchanged;
- no NAND/MTD or `/usr/miyoo` changes;
- recovery policy is unchanged.

## Verification

Focused tests cover missing records, exact format round-trip, malformed-record quarantine, and rejection of invalid writes without replacing a valid committed record.

Architecture guards require Session Manager to consume the persistence service and prohibit direct `FLIP_SESSION_FILE` ownership there.
