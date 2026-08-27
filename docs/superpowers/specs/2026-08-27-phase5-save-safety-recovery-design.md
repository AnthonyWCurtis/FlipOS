# FlipOS Phase 5C Save Safety / Recovery Design

## Goal

Add a user-facing recovery policy on top of the Phase 5B Save Manager so long-running cartridge saves can be restored safely without allowing automatic emulator resume state to silently reintroduce a conflicting timeline.

The first target is Pokemon Yellow on the Miyoo Flip, but the design remains system-agnostic where the underlying core exposes battery-backed save memory.

## Architectural rule

> Manual cartridge-save recovery is a timeline change. FlipOS must preserve the pre-restore timeline, activate the chosen protected save, and prevent an older resume state from being loaded over that restored cartridge save.

Cartridge saves and emulator states remain separate data classes:

- `.sav` is authoritative long-term cartridge progress.
- `.state` is an automatic instant-resume snapshot.
- `.sav.prev` is the previous-known-good cartridge save maintained by Save Manager.
- manual recovery must preserve the currently active cartridge save and its matching resume state before changing timelines.

## Scope

Phase 5C includes:

- Save Safety policy for inspecting current and protected cartridge-save availability.
- Non-destructive manual restore of `.sav.prev`.
- A paired pre-restore recovery set for the current `.sav` and current `.state` where present.
- A restore marker/policy that suppresses automatic state resume until a fresh state is created on the restored timeline.
- An Undo Last Restore path.
- A minimal frontend/GameSwitcher-facing action model: Resume Game, Start Normally, Save Safety.
- Logging and hardware acceptance using Pokemon Yellow.

Phase 5C does not include:

- arbitrary parsing of game-specific save contents;
- screenshot checkpoint timelines;
- multi-generation rotating backups beyond the single protected previous save plus the single pre-restore recovery set;
- cloud backup;
- NAND/MTD writes.

Those may be later features after this recovery workflow is proven.

## User-facing model

For a battery-save game such as Pokemon Yellow, FlipOS should expose three distinct concepts:

```text
Resume Game
Start Normally
Save Safety
```

### Resume Game

Normal instant-resume path:

1. Load protected `.sav` into core battery memory.
2. Restore current `.state` if the resume policy permits it.
3. Continue exactly where the player stopped.

### Start Normally

Normal cartridge boot without state restore:

1. Load protected `.sav` into core battery memory.
2. Skip `.state` restore for this launch.
3. Boot the game normally to its own title/menu flow.

For Pokemon, this allows title screen -> CONTINUE using the cartridge save.

### Save Safety

Present recovery-oriented information and actions. The first milestone only needs structural health/availability, not game-specific save parsing.

Example conceptual UI:

```text
Save Safety

Current Save
Healthy

Previous Protected Save
Available

Restore Previous Save
Undo Last Restore
```

`Undo Last Restore` is only shown when a valid pre-restore recovery set exists.

## Recovery files

Keep the existing Phase 5B files unchanged:

```text
<hash>.sav
<hash>.sav.prev
```

Add a single non-destructive pre-restore recovery set:

```text
<hash>.sav.recovery
<hash>.state.recovery
```

Temporary staged transaction paths may use matching `.tmp` suffixes.

The recovery set represents the active user timeline immediately before the most recent manual restore.

Only one pre-restore recovery generation is required in Phase 5C. A future rotating-backup feature can extend this without changing the basic user model.

## Manual Restore Previous Save policy

Manual restore is an explicit timeline switch.

Preconditions:

1. No emulator instance for the target game is currently running.
2. `.sav.prev` exists and is structurally valid for the expected core save-memory size.
3. Current `.sav`, if present, is structurally valid before it is preserved as recovery data.
4. Existing `.state`, if present, is treated as opaque emulator-state data and preserved alongside the current save before timeline change.

Transaction:

1. Validate `.sav.prev` completely before changing authoritative files.
2. Stage a copy of current `.sav` to `.sav.recovery.tmp` when current `.sav` exists and is structurally valid.
3. Stage a copy of current `.state` to `.state.recovery.tmp` when current `.state` exists and is a regular readable file.
4. Flush/fsync staged recovery files.
5. Commit staged recovery files to `.sav.recovery` and `.state.recovery` by rename.
6. Stage `.sav.prev` as the new current save using the same atomic file rules as Save Manager.
7. Commit restored content to `.sav`.
8. Activate a persistent `resume-blocked-after-save-restore` policy marker for this ROM identity.
9. Report success only after the restored `.sav` and policy marker are committed.

If any required stage fails before authoritative `.sav` promotion, the current `.sav` remains unchanged.

A stale `.tmp` is never authoritative.

## Resume-state quarantine rule

A one-shot `--no-resume` flag is not sufficient for manual recovery.

After a manual cartridge-save restore, the pre-restore `.state` belongs to the old timeline. FlipOS must not offer or automatically load it as the current resume state.

Therefore:

- the old `.state` is preserved as `.state.recovery`;
- the normal current `.state` is removed from the automatic-resume path after its recovery copy is committed;
- a persistent per-ROM restore marker records that automatic resume is blocked;
- launching the game after restore always performs a normal cartridge boot from the newly restored `.sav`;
- once gameplay on the restored timeline reaches a controlled lifecycle save and a fresh `.state` is successfully committed, the restore marker is cleared and normal Resume Game becomes available again.

This prevents accidental timeline crossover even if the user leaves the menu and returns later before launching the game.

## Fresh-state activation

After restore, the next launch behaves like Start Normally.

During that session:

- `.sav` loads normally;
- no pre-restore `.state` is loaded;
- gameplay proceeds from the restored cartridge save;
- controlled lifecycle commit still saves cartridge SRAM first, then state;
- only after both required operations succeed may FlipOS clear the restore marker and consider the new `.state` resumable.

If state creation fails, the restore marker remains active. Cartridge progress remains protected and the next launch again boots normally rather than loading an incompatible state.

## Undo Last Restore

Undo is also an explicit timeline switch.

Preconditions:

- `.sav.recovery` exists and is structurally valid for expected save-memory size.

Transaction:

1. Validate `.sav.recovery` before changes.
2. Preserve the currently active restored `.sav` using the normal protected-current policy so it is not silently lost.
3. Promote `.sav.recovery` atomically back to `.sav`.
4. If `.state.recovery` exists and passes State Manager structural/read validation, restore it to the current `.state` path atomically.
5. Clear the restore marker only after the paired undo transaction is complete.
6. Resume Game may then use the restored pre-restore `.state` because the cartridge save and state timeline have been restored together.

If the recovery state is unavailable or invalid but `.sav.recovery` is valid, FlipOS may still restore the cartridge save but must keep automatic resume blocked and perform a normal cartridge boot. It must never load an unmatched state.

## Interaction with `.sav.prev`

Phase 5B `.sav.prev` remains the automatic previous-known-good copy maintained by controlled save commits.

Manual restore must not reinterpret `.sav.prev` as an editable user slot. It is a protected source candidate.

On restoring `.sav.prev`:

- the old current `.sav` is preserved separately as `.sav.recovery`;
- the selected previous save becomes current `.sav`;
- `.sav.prev` may remain intact until subsequent normal commits rotate it according to Save Manager policy.

This avoids destroying the source recovery candidate during the restore itself.

## Health model

Phase 5C only claims structural health because generic SRAM has no universal semantic validator.

Possible statuses:

- `Healthy`: regular file, exact expected save-memory size, fully readable.
- `Missing`: no file.
- `Invalid`: wrong size, non-regular file, or full read failure.
- `Recovery Available`: valid `.sav.recovery` exists.
- `Resume Blocked`: manual save restore has changed timeline and no fresh matching state has been committed yet.

Do not claim a structurally valid file is semantically valid game progress.

## Component ownership

### Save Manager

Owns:

- `.sav`, `.sav.prev`, `.sav.recovery` identities;
- structural cartridge-save validation;
- atomic copy/promotion operations;
- cartridge recovery transactions.

It does not decide when the user should restore a save.

### State Manager

Owns:

- current `.state` and `.state.recovery` file operations;
- validation/copy/promotion of opaque emulator-state files;
- atomic state-file transactions.

It does not decide resume policy.

### Session Manager / policy layer

Owns:

- Resume Game versus Start Normally behavior;
- persistent per-ROM restore marker;
- whether automatic state resume is permitted;
- coordinating manual restore and undo across Save Manager and State Manager;
- clearing the restore marker only after a fresh matching state is successfully created;
- GameSwitcher integration and user-facing action availability.

This is the correct layer for cross-service timeline policy because neither storage manager should infer player intent.

## Failure handling

Safety priorities, highest first:

1. Never destroy the last known-good cartridge save during recovery.
2. Never auto-load a state that belongs to a different cartridge-save timeline.
3. Never treat temporary files as authoritative.
4. Preserve the pre-restore timeline before activating the chosen older save.
5. Prefer a normal cartridge boot over an ambiguous resume.

If recovery metadata/marker persistence fails, FlipOS must fail conservative: keep automatic resume blocked rather than risk loading a mismatched state.

## Logging

Use explicit prefixes so hardware acceptance can distinguish storage from policy:

```text
FLIP_RECOVERY: RESTORE_REQUESTED
FLIP_RECOVERY: SAVE_RECOVERY_OK
FLIP_RECOVERY: STATE_RECOVERY_OK
FLIP_RECOVERY: RESTORE_OK
FLIP_RECOVERY: RESUME_BLOCKED
FLIP_RECOVERY: FRESH_STATE_OK
FLIP_RECOVERY: RESUME_REENABLED
FLIP_RECOVERY: UNDO_REQUESTED
FLIP_RECOVERY: UNDO_OK
FLIP_RECOVERY: FAILED stage=...
```

Existing `FLIP_SAVE:` and `FLIP_STATE:` logs remain unchanged for their storage operations.

## Hardware acceptance

Pokemon Yellow on the real Miyoo Flip must prove:

1. Existing Phase 5B current `.sav`, `.sav.prev`, and `.state` are valid before test.
2. Manual Restore Previous Save preserves current `.sav` as `.sav.recovery`.
3. Existing current `.state` is preserved as `.state.recovery` when present.
4. Selected `.sav.prev` becomes current `.sav` without modifying internal NAND/MTD.
5. The next launch does not restore the pre-restore `.state`.
6. Pokemon reaches normal boot/title flow and CONTINUE loads the selected restored cartridge progress.
7. Before a fresh state exists, repeated launches continue to avoid the quarantined old state.
8. A successful controlled save creates a fresh `.state` on the restored timeline and re-enables Resume Game.
9. Resume Game then restores that fresh state normally.
10. Undo Last Restore restores the preserved pre-restore `.sav` and its matching `.state` when both are valid.
11. Failed recovery staging leaves authoritative current save recoverable.
12. START+SELECT, lid suspend/resume, GameSwitcher exit code 20, RK817 deep-resume recovery, 32768-to-32000 audio conversion and established frame timing remain unchanged.

## Future rotating backups

Do not add multi-generation rotation in Phase 5C.

After manual recovery UX has been proven, a later feature may retain timestamped generations such as daily or lifecycle checkpoints, subject to clear retention limits and storage policy. The Phase 5C `.sav.recovery` concept can remain the immediate undo slot even if longer history is added later.

## Summary

The Phase 5C recovery model is deliberately conservative:

- automatic `.sav.prev` protects against failed/corrupt current saves;
- manual restore preserves the current save and state as a paired recovery timeline;
- the selected previous cartridge save becomes authoritative;
- the old automatic resume state is quarantined;
- normal cartridge boot is forced until a fresh matching state exists;
- Undo Last Restore can return to the paired pre-restore timeline.

This provides a simple player-facing recovery flow while preventing the most dangerous failure mode: restoring an older cartridge save and then silently loading a newer or mismatched emulator state over it.
