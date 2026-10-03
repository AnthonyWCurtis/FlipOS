# FlipOS Phase 6A — Persistent Session Registry

Date: 2026-10-03
Status: hardware accepted
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

Runtime integration acceptance:

- legacy `state/session/current` remains authoritative
- running/suspended/interrupted/signaled/error states mirror into the registry
- exited/closed sessions are removed from the registry
- registry write failures are non-fatal to the existing session path

Hardware acceptance:

- launch a GB title and observe a running registry entry
- trigger GameSwitcher suspend and observe the same entry become suspended
- resume and exit normally, then confirm the registry entry is removed
- verify lid close/open automatic resume still bypasses Game Options
- verify save/state persistence and RK817 post-resume audio remain unchanged

### 2026-10-03 hardware result

Device: Miyoo Flip RK3566, Linux 5.10.160, AArch64.

ROM used:

```text
/mnt/SDCARD/Roms/GB/Pokemon - Blue Version (USA, Europe).gb
```

Observed lifecycle:

```text
launch       -> registry state=running
MENU         -> registry state=suspended
resume       -> registry state=running
START+SELECT -> current state=exited, registry entry removed
```

The registry contained a single `(GB, ROM)` entry throughout launch/suspend/resume; upsert did not create duplicates.

Lid regression passed:

- emulator returned suspend exit code 20
- registry mirrored `state=suspended`
- frontend entered deep sleep
- lid open triggered `FRONTEND_STANDBY auto resume`
- Save Safety reported `FLIP_SAVE_SAFETY: AUTOMATIC_RESUME`
- RK817 exact broken signature `17=06 38=00 40=69` was detected
- RK817 deep-resume recovery was applied successfully
- cartridge save loaded successfully
- current savestate restored successfully
- registry returned to `state=running`
- frame pacing remained around the expected ~59.7 FPS target

UX observations retained for later Phase 6 GameSwitcher work:

- deliberate GameSwitcher resume currently requires selecting the title and then confirming Resume Game in Game Options
- START+SELECT exits the emulator cleanly and returns to GameSwitcher

These are existing frontend navigation behaviours and are not Phase 6A registry failures.
