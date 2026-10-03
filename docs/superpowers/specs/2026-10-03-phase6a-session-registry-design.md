# FlipOS Phase 6A — Persistent Session Registry

Date: 2026-10-03
Status: implementation branch
Branch: `phase6/session-registry`

## Goal

Introduce a small persistent multi-session registry that can become the source of truth for GameSwitcher without disturbing the hardware-tested Phase 5C single-current-session path.

Phase 6A is deliberately additive. The existing `state/session/current` record, lid-wake automatic resume, Save Safety, save/state transactions and exit-code handling remain unchanged until later Phase 6 milestones migrate them explicitly.

## Storage

Default path:

```text
/mnt/SDCARD/flipos-dev/state/session/registry
```

The test build overrides the root with `FLIP_SESSION_REGISTRY_ROOT`.

The registry is versioned text and is committed atomically using:

1. write `registry.tmp`
2. `fflush()`
3. file-data `fsync()`
4. close
5. rename to `registry`

This matches FlipOS's existing FAT32-safe transaction policy without claiming journaled-filesystem durability.

## Identity

A session is uniquely identified by:

```text
(system, rom)
```

Each entry stores:

- system id
- ROM path
- emulator program path
- lifecycle state
- original start timestamp
- last-updated timestamp

The registry is bounded to 16 entries.

## API

`flip_session_registry_load()`
: Read and validate the complete registry.

`flip_session_registry_upsert()`
: Insert or replace a session and move it to the newest position.

`flip_session_registry_remove()`
: Remove one session without deleting emulator save/state data.

`flip_session_registry_find()`
: Locate one session by system and ROM.

## Safety rules

- No NAND/MTD changes.
- No save-state or cartridge-save deletion.
- A malformed registry fails closed rather than partially accepting untrusted entries.
- Newline-bearing string fields are rejected.
- Phase 5C `current` session metadata remains authoritative for existing runtime behaviour during Phase 6A.

## Acceptance

Host acceptance:

- empty registry loads successfully
- first session persists
- multiple sessions persist
- upsert replaces rather than duplicates
- updated session moves to newest position
- remove is idempotent
- find works
- malformed records fail closed
- `make verify` passes

Hardware acceptance is intentionally deferred until the registry is wired into actual frontend/session lifecycle behaviour in the next Phase 6 slice.
