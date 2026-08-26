# Phase 5 State Manager Design

## Status

Approved architecture for the first Phase 5 Save / State milestone.

This design introduces a shared FlipOS State Manager for emulator save states. It deliberately does not redesign GameSwitcher, add another emulator core, change battery-backed in-game save handling, or change the existing Gambatte raw state format.

## Goal

Move save-state storage policy out of `flip_gb.c` into a shared runtime service while preserving existing Game Boy resume states and adding a safe previous-known-good recovery path.

The architectural rule is:

> Cores serialize emulator state. FlipOS owns state identity, storage, validation, recovery, and lifecycle integration.

## Constraints

- SD-card-only development.
- No internal NAND/MTD writes.
- Preserve the stock Miyoo recovery path.
- Keep `main` known-good and hardware-tested.
- Work on `phase5/state-manager` until hardware acceptance passes.
- Preserve the current GB state directory, ROM-path hash, file name, and raw Gambatte state bytes.
- Preserve Lifecycle Host save semantics and emulator exit code 20.
- Do not redesign GameSwitcher in this phase.
- Do not add GBA or another core.
- Do not introduce a database or background worker.
- Battery-backed game saves (`.sav`/SRAM) are not migrated by this first State Manager milestone. They require a separate Save Manager policy later in Phase 5/7.

## Existing Compatibility Contract

Current Game Boy resume states live at:

```text
/mnt/SDCARD/flipos-dev/states/gb/<fnv1a64-rom-path>.state
```

The hash is 64-bit FNV-1a over the ROM path string. The file itself is the raw output of the core serialize API with no FlipOS header.

Phase 5 keeps all three properties unchanged for the primary state:

1. directory;
2. hash/file name;
3. raw serialized payload.

An existing pre-Phase-5 `.state` file therefore remains directly loadable after migration.

## Architecture

```text
Frontend / Session policy
          |
          v
    Core Host / Lifecycle
          |
          | save / restore request
          v
      State Manager
          |
          +-- ROM identity + path policy
          +-- serialize-size validation
          +-- atomic write transaction
          +-- current state
          +-- previous-known-good state
          +-- recovery/fallback policy
          |
          v
   Core Adapter serialize/unserialize
```

The State Manager is core-neutral. It receives a `flip_core_adapter_t`, a system identifier, and the ROM path. It must not include libretro or Gambatte-specific types.

## Public State Manager Surface

The first implementation should expose a small API equivalent to:

```c
typedef struct {
    const char *system_id;
    const char *rom_path;
} flip_state_context_t;

int flip_state_save(
    flip_core_adapter_t *core,
    const flip_state_context_t *context);

int flip_state_restore(
    flip_core_adapter_t *core,
    const flip_state_context_t *context,
    flip_state_restore_result_t *result);
```

The exact names may be adjusted during implementation, but responsibilities must remain narrow.

Restore results must distinguish at least:

- no state exists;
- current state restored;
- previous-known-good state restored;
- state files exist but none can be restored;
- storage/read error.

This lets the host make correct lifecycle decisions without parsing log strings.

## Identity and Paths

### System namespace

State files remain namespaced by system. For the existing Game Boy path, the system identifier maps to `gb`.

### ROM identity

For compatibility, Phase 5 continues using the existing 64-bit FNV-1a hash of the ROM path string. ROM-content hashing is not introduced in this migration.

Changing identity to a content hash can be considered later only with an explicit migration strategy.

### Files

For a primary path:

```text
<hash>.state
```

State Manager may use these sibling transaction/recovery files:

```text
<hash>.state.tmp
<hash>.state.prev
<hash>.state.prev.tmp
```

The primary `.state` remains a raw core state and retains its historical location and format.

## Validation Model

Because the primary state format cannot change without breaking compatibility, validation is intentionally layered.

### Structural validation

Before attempting restore:

- the core serialize size must be non-zero;
- candidate file size must exactly equal the current core serialize size;
- the full candidate must be readable.

### Semantic validation

The strongest available validation is the core's own `unserialize` operation.

A candidate becomes **known good** only after `flip_core_unserialize()` succeeds for that candidate during a real restore.

No checksum sidecar is required for the first milestone. A checksum can detect torn bytes but cannot prove that a state is semantically restorable, and a multi-file metadata transaction would add complexity on FAT32 without improving the primary compatibility requirement.

## Previous-Known-Good Semantics

`*.state.prev` must mean **successfully restored previously**, not merely "the file that happened to exist before the last write."

This distinction prevents a corrupt current state from being blindly rotated into the recovery slot.

The State Manager tracks, for the running session, whether the current state was successfully restored.

### On successful restore of current

- mark the current candidate as known good for this session;
- continue normal execution;
- do not rewrite files merely because restore succeeded.

### On a later successful save in that same session

If the session began by successfully restoring the current `.state`:

1. copy the known-good current state to `.state.prev.tmp`;
2. flush and `fsync` the copy;
3. rename `.state.prev.tmp` to `.state.prev`;
4. write the newly serialized state to `.state.tmp`;
5. flush and `fsync` the new state;
6. rename `.state.tmp` to `.state`.

If the session did **not** successfully restore the current `.state`, the State Manager does not overwrite `.state.prev` with an unproven state.

This means the recovery slot may intentionally lag behind the newest state by a session. Safety is preferred over freshness.

## Save Transaction

A save must never write directly into the current `.state`.

Required sequence:

1. ask the core for serialize size;
2. allocate a bounded buffer of exactly that size;
3. serialize into memory;
4. ensure the state directory exists;
5. preserve known-good current to `.prev` when eligible;
6. open `.state.tmp` for replacement;
7. write the entire serialized payload;
8. `fflush` and `fsync` the file;
9. close it successfully;
10. verify the temp file has the expected size;
11. rename `.state.tmp` to `.state`;
12. report save success only after the rename succeeds.

If any step fails:

- the existing `.state` must remain usable whenever it has not already been replaced;
- `.state.prev` must never be destroyed as part of failure cleanup;
- temporary files may be removed best-effort;
- Lifecycle Host receives save failure and retains its existing policy.

Directory `fsync` may be attempted where supported, but failure caused specifically by a filesystem not supporting directory sync must not by itself turn an otherwise completed FAT32 save into a false failure. File-data `fsync` remains required.

## Restore and Recovery Policy

Restore order is deterministic:

1. current `.state`;
2. previous-known-good `.state.prev` only if current cannot be structurally or semantically restored.

For each candidate:

1. require exact expected size;
2. read the complete file;
3. call core `unserialize`;
4. accept only on success.

If current succeeds, return `CURRENT_RESTORED`.

If current fails and previous succeeds:

- return `PREVIOUS_RESTORED`;
- log that recovery occurred;
- do **not** immediately overwrite the corrupt current file during restore;
- the running session is now based on the previous-known-good state, and a later normal save may establish a new current state.

If both fail, return a distinct unusable-state result. The host must not pretend that no state existed.

## Startup Transaction Cleanup

A previous crash may leave `.tmp` files.

At startup/restore:

- `.state` and `.state.prev` are authoritative candidates;
- `.tmp` files are never automatically preferred over committed files;
- stale temp files may be deleted best-effort after committed candidates are inspected;
- no speculative recovery from temp files is part of this first milestone.

This keeps crash recovery deterministic and avoids treating an incompletely committed write as valid.

## Core Host and Lifecycle Integration

The existing Core Host frame/lifecycle order remains unchanged.

The temporary Game Boy callbacks in `flip_gb.c` are replaced by State Manager calls, but the Core Host continues to see the same logical contract:

- save returns success/failure;
- restore returns whether a state was actually restored;
- suspend and GameSwitcher only exit with code 20 after successful save;
- START+SELECT still attempts save but exits normally even if save fails;
- restored sessions still resync Timing Host before entering the frame loop.

No lifecycle policy moves into the State Manager.

## `flip_gb.c` After Migration

`flip_gb.c` should retain composition only:

- validate argv;
- choose the Gambatte adapter;
- provide `system_id = "gb"` and ROM path;
- configure Core Host with shared State Manager callbacks/context;
- run the host;
- return the host-selected exit code.

The following logic moves out of `flip_gb.c`:

- FNV state-path hashing implementation;
- directory creation;
- temp-file write/fsync/rename;
- raw state file reads;
- size validation;
- recovery selection.

## Logging

State Manager logs use a stable `FLIP_STATE:` prefix.

Useful events include:

```text
FLIP_STATE: NO_STATE
FLIP_STATE: SAVE_OK path=... size=...
FLIP_STATE: RESTORE_CURRENT_OK path=... size=...
FLIP_STATE: CURRENT_INVALID ...
FLIP_STATE: RESTORE_PREVIOUS_OK path=... size=...
FLIP_STATE: RESTORE_UNUSABLE ...
FLIP_STATE: SAVE_FAILED stage=...
```

Logs must distinguish storage failures from core unserialize rejection.

## Error Handling Principles

- Never delete `.prev` merely because `.state` is bad.
- Never label an untested state "known good."
- Never return "no state" when files exist but are corrupt/unusable.
- Never change exit-code policy inside storage code.
- Never modify NAND/MTD.
- Never silently migrate or rewrite a valid legacy state on load.

## Testing Strategy

### Native State Manager tests

Use a fake core adapter and temporary host filesystem directory to cover:

- exact legacy FNV path compatibility;
- no-state restore;
- valid current restore;
- current size mismatch;
- current unserialize rejection;
- fallback to valid previous state;
- both candidates unusable;
- short read/write failure handling where practical;
- temp-file save followed by atomic rename;
- failed save preserving existing current;
- known-good current copied to previous before replacement;
- unverified current never replacing previous;
- stale temp files ignored as authoritative restore candidates.

### Architecture tests

Verify:

- `flip_gb.c` no longer implements hashing or file I/O for states;
- State Manager public headers contain no Gambatte/libretro types;
- Core Host lifecycle order and exit code 20 semantics remain unchanged.

### Full regression

Run all existing controls, timing, audio/resampler, lifecycle, core, Core Host, diagnostics, telemetry, architecture tests, and the ARM64 static build.

## Hardware Acceptance

Deploy SD-only and validate on the real Miyoo Flip:

1. an existing pre-Phase-5 GB state resumes correctly;
2. a new state saves and resumes correctly;
3. suspend/lid resume still works;
4. GameSwitcher request path still saves and returns exit code 20;
5. START+SELECT behavior is unchanged;
6. audio remains 32768 source -> 32000 physical output with working RK817 deep-resume recovery;
7. timing remains near the verified ~59.7 FPS behavior;
8. deliberately invalid current state falls back to a previously proven `.prev` state in a controlled test;
9. failed save does not destroy the previously committed recoverable state.

Corruption tests must use only SD state files. Internal firmware/NAND/MTD is never involved.

## Out of Scope

- Battery-backed SRAM / `.sav` backup policy.
- User-facing recovery UI.
- Multiple rotating generations beyond one previous-known-good state.
- Cloud backup/sync.
- Content-hash ROM identity migration.
- Compression.
- Encryption.
- GameSwitcher/session database redesign.
- Second emulator core.

These can build on the State Manager after the storage contract is proven.

## Success Criteria

Phase 5 State Manager milestone is complete when:

- state persistence is core-neutral and no longer implemented in `flip_gb.c`;
- existing GB state paths and raw payloads remain compatible;
- writes are temp-file + file-fsync + rename transactions;
- previous-known-good is only sourced from a state that successfully restored;
- restore can fall back to that previous-known-good state;
- lifecycle exit/save semantics are unchanged;
- full WSL regression/build passes;
- real Miyoo Flip legacy restore, save/resume, suspend/resume, recovery fallback, audio, and timing validation pass;
- only then may the verified milestone be promoted to `main`.
