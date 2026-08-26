# Phase 5B Save Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add true, protected battery-backed cartridge-save persistence to FlipOS, independent of emulator state snapshots.

**Architecture:** Extend the Core Adapter with generic battery-memory access, implement it in Gambatte through libretro SAVE_RAM, then add a shared Save Manager that owns hashed paths, structural validation, current/previous recovery and atomic FAT32-safe commits. Core Host coordinates save-memory load/flush with existing state/lifecycle callbacks without changing exit-code policy.

**Tech Stack:** C11 host tests, AArch64 static production build, libretro Gambatte, FAT32/vfat SD storage, existing FlipOS Core Host/State Manager/Lifecycle Host.

**Spec:** `docs/superpowers/specs/2026-08-26-phase5-save-manager-design.md`

## Global Constraints

- SD-card-only development; no internal NAND/MTD writes.
- `.sav` is authoritative long-term cartridge progress; `.state` is resume/rewind data.
- Loading a state must not itself write or replace protected `.sav` data.
- Preserve existing START+SELECT, GameSwitcher exit-code-20, lid suspend/resume, audio recovery and timing behavior.
- Use exact-ROM-path FNV-1a64 identity, consistent with State Manager.
- File-data `fflush()` + `fsync()` is required before rename; do not claim journaled durability on FAT32/vfat.
- No screenshot checkpoint timeline UI in Phase 5B.

---

### Task 1: Generic Core battery-memory interface

**Files:**
- Modify: `include/flip_core.h`
- Modify: `src/runtime/flip_core.c`
- Modify: `tests/test_flip_core.c`

**Interfaces:**
- Produces: `void *flip_core_save_memory_data(flip_core_adapter_t *adapter)`
- Produces: `size_t flip_core_save_memory_size(flip_core_adapter_t *adapter)`
- Adds adapter callbacks `save_memory_data` and `save_memory_size`.

- [ ] Add RED tests using a fake adapter that returns a fixed byte array and size; also verify missing callbacks return `NULL`/`0` without invalidating a core.
- [ ] Run `make test-core`; expected RED because wrappers/callback fields do not exist.
- [ ] Add optional callback fields to `flip_core_adapter_t` and wrappers that safely return `NULL`/`0` if unsupported.
- [ ] Keep `flip_core_validate()` requirements unchanged for battery memory because battery-less systems are valid.
- [ ] Run `make test-core`; expected GREEN.
- [ ] Commit `Expose core battery save memory`.

### Task 2: Gambatte SAVE_RAM bridge

**Files:**
- Modify: `src/emulators/flip_gambatte_core.c`
- Modify/Create architecture test under `tests/` as appropriate.

**Interfaces:**
- Consumes: Task 1 battery-memory callbacks.
- Produces: Gambatte adapter battery memory through `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` and `retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)`.

- [ ] Add RED architecture assertions requiring both libretro SAVE_RAM calls in `flip_gambatte_core.c` and requiring adapter callback assignments.
- [ ] Run the architecture test; expected RED.
- [ ] Implement `gambatte_save_memory_data()` and `gambatte_save_memory_size()` guarded by loaded-game state.
- [ ] Wire them into the static adapter.
- [ ] Run architecture test and `make test-core`; expected GREEN.
- [ ] Commit `Expose Gambatte battery save RAM`.

### Task 3: Shared Save Manager path and load behavior

**Files:**
- Create: `include/flip_save.h`
- Create: `src/runtime/flip_save.c`
- Create: `tests/test_flip_save.c`
- Modify: `Makefile`

**Interfaces:**
- Produces: `flip_save_context_t` containing system, ROM path and FNV-1a64 identity.
- Produces: `void flip_save_context_init(flip_save_context_t *, const char *system, const char *rom_path)`.
- Produces: `flip_save_load_result_t flip_save_load(flip_core_adapter_t *, flip_save_context_t *)`.
- Produces: `int flip_save_commit(flip_core_adapter_t *, flip_save_context_t *)` in Task 4.
- Test root override: `FLIP_SAVE_ROOT`, default `/mnt/SDCARD/flipos-dev/saves`.

- [ ] Add RED tests freezing the same ROM-path FNV-1a contract used by State Manager and checking `.sav`, `.sav.prev`, `.sav.tmp`, `.sav.prev.tmp` paths under a temporary root.
- [ ] Add RED load tests: no memory => no-op; no file => NO_SAVE; exact-size current => bytes copied; wrong-size current + valid previous => previous loaded; invalid current/previous => UNUSABLE; non-regular file => ERROR.
- [ ] Add Makefile `test-save` target using `-DFLIP_SAVE_ROOT='"/tmp/flipos-save-test"'`.
- [ ] Run `make test-save`; expected RED linker/compile failures.
- [ ] Implement context/path helpers and structural load validation. Never copy partial/unvalidated file bytes into core memory.
- [ ] Log `FLIP_SAVE:` results.
- [ ] Run `make test-save`; expected GREEN.
- [ ] Commit `Add Save Manager loading`.

### Task 4: Atomic protected `.sav` commits

**Files:**
- Modify: `src/runtime/flip_save.c`
- Modify: `tests/test_flip_save.c`

**Interfaces:**
- Produces: `int flip_save_commit(flip_core_adapter_t *core, flip_save_context_t *context)` returning 0 for success/no-memory and -1 for storage failure.

- [ ] Add RED tests for initial save creation, update preserving previous valid current, invalid current not replacing previous, serialize-memory pointer/size failures, and forced `.sav.tmp` open failure preserving current/previous.
- [ ] Run `make test-save`; expected RED.
- [ ] Implement `.sav.prev.tmp` preservation and `.sav.tmp` commit with full writes, `fflush()`, `fsync()`, close, size verification and rename.
- [ ] Never remove `.sav.prev` on failure.
- [ ] Attempt directory fsync only as best effort; keep file-data fsync mandatory.
- [ ] Run `make test-save`; expected GREEN.
- [ ] Commit `Add atomic battery save commits`.

### Task 5: Core Host save-memory lifecycle integration

**Files:**
- Modify: `include/flip_core_host.h`
- Modify: `src/runtime/flip_core_host.c`
- Modify: `src/emulators/flip_gb.c`
- Modify: `tests/test_flip_core_host.c`
- Add/update architecture tests.

**Interfaces:**
- Add Core Host callbacks `load_save` and `commit_save` using existing core-host callback shape.
- `flip_gb.c` owns only Save Manager context/composition, not file I/O.

- [ ] Add RED Core Host tests proving save-memory load occurs after core game load but before state restore/frame loop; state restore alone does not invoke save commit.
- [ ] Add RED lifecycle tests proving controlled save paths call cartridge commit and state save while preserving existing exit semantics.
- [ ] Add architecture test forbidding direct save-file I/O and libretro memory calls in `flip_gb.c`.
- [ ] Run focused tests; expected RED.
- [ ] Initialize `flip_save_context_t` in `flip_gb.c` and provide load/commit bridge callbacks.
- [ ] Core Host calls battery-save load before state restore.
- [ ] Integrate cartridge commit into controlled lifecycle saves without changing exit code 20 or START+SELECT exit policy.
- [ ] Run `make test-core-host make test-lifecycle make test-save make test-architecture`; expected GREEN.
- [ ] Commit `Route battery saves through Save Manager`.

### Task 6: One-launch boot without state restore

**Files:**
- Modify: `src/emulators/flip_gb.c` and/or `include/flip_core_host.h` / `src/runtime/flip_core_host.c`, selecting the narrowest boundary during implementation.
- Modify: focused tests.

**Interfaces:**
- Provide an internal launch option `--no-resume` for `flip-gb` that skips State Manager restore for that launch only.
- Cartridge `.sav` load still occurs normally.
- Existing `.state` and `.state.prev` files remain untouched.

- [ ] Add RED test/architecture assertion for the no-resume launch behavior.
- [ ] Implement `flip-gb <rom> --no-resume` argument parsing or equivalent narrow config flag.
- [ ] Ensure cartridge save load is not skipped.
- [ ] Run focused tests and build; expected GREEN.
- [ ] Commit `Add boot without resume state option`.

### Task 7: Full software verification and roadmap update

**Files:**
- Modify: `docs/ROADMAP.md` only after software gate is green; hardware completion remains unchecked until device tests pass.

- [ ] Run clean full suite: `make clean`, all existing test targets, `make test-save`, `make test-architecture`, `make all`, `git diff --check`.
- [ ] Confirm only the six preserved diagnostic files remain untracked in the user's WSL checkout after pull/build.
- [ ] Record exact candidate `build/flip-gb` SHA-256.
- [ ] Commit software-ready roadmap status.

### Task 8: Real-Miyoo-Flip hardware acceptance

**Files:**
- Documentation update only after acceptance.

- [ ] Atomically deploy candidate `flip-gb` to SD and verify device SHA equals local SHA.
- [ ] Launch Pokemon Yellow and verify `FLIP_SAVE` reports nonzero SRAM size / functional memory path.
- [ ] Make a normal in-game Pokemon save, then controlled exit; verify hashed `.sav` appears with expected size and SHA.
- [ ] Relaunch with `--no-resume`; verify title screen appears and Pokemon `Continue` loads the expected cartridge progress.
- [ ] Relaunch normally; verify automatic `.state` resume still works.
- [ ] Make another in-game save and controlled exit; verify current `.sav` changes and `.sav.prev` preserves prior structurally valid current.
- [ ] Back up both files to host, truncate only current `.sav`, launch without resume, require `CURRENT_INVALID` + `LOAD_PREVIOUS_OK`, and visually verify previous cartridge save loads.
- [ ] Restore files from host backup.
- [ ] Exercise lid suspend/resume, GameSwitcher exit code 20, START+SELECT, RK817 recovery/audio 32768->32000 and timing near target.
- [ ] Update roadmap with exact hardware-tested code commit and binary SHA, then run final exact-head software verification before promotion to `main`.
