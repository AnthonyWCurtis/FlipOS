# FlipOS Phase 5B Save Manager Design

## Goal

Add true cartridge battery-save persistence to FlipOS so games such as Pokemon can use their normal in-game Save/Continue flow independently of emulator save states, while protecting long-term progress against interrupted writes and accidental rollback from old save states.

## Architectural rule

> Cores expose battery-backed memory. FlipOS owns battery-save identity, loading, validation, atomic persistence, backup policy, and lifecycle integration.

Battery-backed saves and emulator states are different data classes:

- `.sav` is authoritative long-term cartridge progress.
- `.state` is a whole-emulator resume or rewind snapshot.
- Loading an old `.state` must never silently replace a newer protected `.sav` on disk.

## Scope

Phase 5B includes:

- Core Adapter API support for battery-backed save memory.
- Gambatte implementation using libretro `RETRO_MEMORY_SAVE_RAM`.
- Shared Save Manager for path identity, load, atomic commit, validation and previous backup.
- GB integration through the existing Core Host/lifecycle path.
- A safe boot-without-resume-state test path for validating native game Save/Continue behavior.
- Hardware verification using Pokemon Yellow.

Phase 5B does not include:

- Screenshot checkpoint timeline UI.
- GameSwitcher checkpoint browser.
- User-facing recovery UI.
- Arbitrary per-game RAM parsing.
- NAND/MTD writes.

Those remain later work, with screenshot checkpoints planned after save protection is proven.

## Save identity and paths

Use the same ROM identity rule already established by the State Manager: 64-bit FNV-1a of the exact ROM path string.

Production root:

```text
/mnt/SDCARD/flipos-dev/saves/<system>/
```

For GB Pokemon Yellow this becomes:

```text
/mnt/SDCARD/flipos-dev/saves/gb/<rom-hash>.sav
/mnt/SDCARD/flipos-dev/saves/gb/<rom-hash>.sav.tmp
/mnt/SDCARD/flipos-dev/saves/gb/<rom-hash>.sav.prev
/mnt/SDCARD/flipos-dev/saves/gb/<rom-hash>.sav.prev.tmp
```

The protected save name is hash-based rather than ROM-filename-based so identity is stable with unusual characters and consistent with State Manager behavior.

## Core Adapter API

Extend `flip_core_adapter_t` with battery-memory accessors:

```c
void *(*save_memory_data)(void *userdata);
size_t (*save_memory_size)(void *userdata);
```

Shared wrappers in `flip_core.[ch]` expose these safely to Save Manager.

Gambatte implements them using:

```c
retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)
retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)
```

A core with no battery memory returns `NULL` and/or size `0`; Save Manager treats that as no cartridge save support, not as an error.

## Load policy

After the ROM is loaded and before normal gameplay resumes, Save Manager asks the core for battery-memory size and pointer.

If no battery memory exists, loading is a no-op success.

If `<hash>.sav` does not exist, SRAM is left at the core's initialized value.

If `<hash>.sav` exists:

1. File must be a regular file.
2. File size must exactly match the current core save-memory size.
3. The full file must be read successfully.
4. Bytes are copied into the core-provided save-memory buffer only after the full file has been read and validated structurally.

If current `.sav` is invalid and `.sav.prev` is valid, load `.sav.prev` instead.

A stale `.tmp` file is never authoritative.

Because generic SRAM has no universal semantic validator equivalent to `retro_unserialize()`, validation is structural: expected size, regular file, full read. The previous copy therefore protects against interrupted/partial replacement, not every possible game-level logical corruption.

## Save policy

Battery memory is persisted independently of emulator states.

A save commit:

1. Ask core for save-memory pointer and exact size.
2. If no battery memory, return no-op success.
3. Ensure save directory exists.
4. If current `.sav` exists and is structurally valid, copy it to `.sav.prev.tmp`, flush/fsync, then rename to `.sav.prev`.
5. Write current SRAM bytes to `.sav.tmp`.
6. Require a full write.
7. `fflush()` and `fsync()` file data.
8. Close file.
9. Verify `.sav.tmp` has expected size.
10. Rename `.sav.tmp` to `.sav`.
11. Report success only after rename.

Never overwrite `.sav` directly.

Never destroy `.sav.prev` during failed cleanup.

Directory fsync may be attempted, but FlipOS must not claim journaled durability on FAT32/vfat.

## Lifecycle integration

Battery-save flush and state-save are separate operations.

For all controlled exits that already save emulator state — START+SELECT, GameSwitcher request, lid suspend — FlipOS must also commit SRAM before process teardown.

Battery-save failure is treated conservatively:

- Suspend/GameSwitcher: do not report successful lifecycle completion if protected SRAM commit fails.
- START+SELECT: preserve the existing user expectation that the process can exit, but log the save failure prominently. The previously committed `.sav` remains intact because writes are atomic.

On normal startup, cartridge `.sav` is loaded before State Manager restores `.state`.

This order ensures the core begins with the protected cartridge save available. A state restore may alter in-memory SRAM as part of the snapshot, but that older in-memory SRAM is not automatically committed to disk merely because the state was loaded.

## Critical rollback rule

Loading a save state is not permission to overwrite the protected cartridge save.

Therefore:

- Restore does not write `.sav`.
- Automatic resume does not write `.sav` during load.
- A later explicit/controlled lifecycle save may commit the current in-memory SRAM, because at that point the player has intentionally continued from that timeline.
- Future checkpoint UX should warn/label when rewinding to an older timeline, but Phase 5B remains storage-focused.

This avoids immediate silent rollback while preserving normal emulator semantics once the player resumes playing from an old state.

## Boot without resume state

Add a narrow host/session option that skips State Manager restore for one launch while leaving all state files untouched.

Purpose:

- reach title screens,
- test Pokemon's native Continue flow,
- access game modes hidden by automatic resume,
- verify `.sav` independently from `.state`.

Phase 5B only needs an internal/testable launch flag or environment option; polished frontend/GameSwitcher presentation comes later.

## Logging

Use prefix `FLIP_SAVE:`.

Examples:

```text
FLIP_SAVE: NO_MEMORY
FLIP_SAVE: NO_SAVE
FLIP_SAVE: LOAD_CURRENT_OK path=... size=...
FLIP_SAVE: CURRENT_INVALID path=... reason=size actual=... expected=...
FLIP_SAVE: LOAD_PREVIOUS_OK path=... size=...
FLIP_SAVE: SAVE_OK path=... size=...
FLIP_SAVE: SAVE_FAILED stage=...
```

## Hardware acceptance

Pokemon Yellow on real Miyoo Flip must prove:

1. Gambatte exposes nonzero `RETRO_MEMORY_SAVE_RAM` size.
2. A protected `.sav` is created on SD after an in-game Pokemon save and controlled exit.
3. `.sav` survives normal restart.
4. Launch with resume-state skipped reaches Pokemon title screen.
5. Pokemon's own `Continue` loads the expected cartridge save.
6. Existing automatic `.state` resume still works.
7. Controlled save updates `.sav` atomically and preserves `.sav.prev`.
8. Deliberately truncated current `.sav` falls back to valid `.sav.prev` without touching NAND/MTD.
9. Failed `.sav.tmp` write does not destroy current or previous save.
10. Lid suspend/resume, GameSwitcher exit code 20, START+SELECT, audio recovery and timing remain unchanged.

## Future checkpoint timeline

After Phase 5B is proven, FlipOS can add screenshot checkpoints as a separate feature:

- automatic Resume remains single latest state,
- manual checkpoints form a small timeline,
- each checkpoint has screenshot, timestamp and optional user label,
- old checkpoints may rotate while pinned checkpoints are retained,
- checkpoint restore never directly edits protected `.sav` on disk.

This later feature builds on, but does not belong inside, the Save Manager.