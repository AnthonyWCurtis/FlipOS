# Phase 5C Save Safety / Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add non-destructive cartridge-save recovery, paired save/state undo, persistent resume blocking after a timeline restore, and minimal frontend recovery actions on top of the proven Phase 5B Save/State Managers.

**Architecture:** Keep storage mechanics in Save Manager and State Manager, and add a focused recovery/session-policy layer that coordinates cross-file timeline switches and the persistent per-ROM `.restore-pending` marker. Recovery actions that require the core's exact SRAM size are executed by the system runner (`flip-gb`) in narrow management modes; the frontend stays generic and never parses GB save files or constructs save/state paths. Core Host clears the marker only after both cartridge save and fresh state commit succeed.

**Tech Stack:** C11 host tests, AArch64 static production build, existing FlipOS Save Manager, State Manager, Core Host, Session Manager, frontend, Gambatte/libretro, FAT32/vfat SD storage.

**Spec:** `docs/superpowers/specs/2026-08-27-phase5-save-safety-recovery-design.md`

## Global Constraints

- SD-card-only development; no internal NAND/MTD writes.
- `.sav` remains authoritative long-term cartridge progress; `.state` remains automatic instant-resume state.
- Manual save restore is a timeline switch and must preserve the pre-restore timeline before activating the chosen older save.
- Never auto-load a state that may belong to a different cartridge-save timeline.
- A stale `.state.recovery` must never be paired with a newer `.sav.recovery` generation.
- Persistent marker path: `/mnt/SDCARD/flipos-dev/sessions/<system>/<rom-hash>.restore-pending`.
- Marker presence forbids automatic state resume; marker read failure is conservative and also forbids resume.
- Clear the marker only after cartridge save commit and fresh state commit both succeed on the restored timeline.
- Preserve existing START+SELECT behavior, GameSwitcher exit code 20, lid suspend/resume, RK817 recovery, 32768->32000 audio conversion and established timing behavior.
- Use the frozen exact-ROM-path FNV-1a64 identity already used by Save Manager and State Manager.
- File-data `fflush()` + `fsync()` before rename remains mandatory; do not claim journaled durability on FAT32/vfat.
- Phase 5C keeps one `.sav.prev` plus one immediate `.sav.recovery`/`.state.recovery` generation only; no rotating history yet.

---

### Task 1: Save Manager recovery primitives and structural health

**Files:**
- Modify: `include/flip_save.h`
- Modify: `src/runtime/flip_save.c`
- Modify: `tests/test_flip_save.c`

**Interfaces:**
```c
typedef enum {
    FLIP_SAVE_HEALTH_MISSING = 0,
    FLIP_SAVE_HEALTH_HEALTHY,
    FLIP_SAVE_HEALTH_INVALID,
    FLIP_SAVE_HEALTH_ERROR
} flip_save_health_t;

flip_save_health_t flip_save_health(
    flip_core_adapter_t *core,
    const flip_save_context_t *context,
    const char *suffix);

int flip_save_restore_previous(
    flip_core_adapter_t *core,
    const flip_save_context_t *context);

int flip_save_restore_recovery(
    flip_core_adapter_t *core,
    const flip_save_context_t *context);
```
Recovery suffixes are `.sav.recovery` and `.sav.recovery.tmp`.

- [ ] Add RED path tests for `.sav.recovery` and `.sav.recovery.tmp` using Pokemon Yellow identity `792f9628ba43b7d9`.
- [ ] Add RED health tests for missing, exact-size/readable, wrong-size, non-regular and read-failure candidates.
- [ ] Add RED restore-previous test: current A and previous B must finish with recovery A, current B, previous still B, and no authoritative temp files.
- [ ] Add RED forced `.sav.recovery.tmp` failure test proving current/previous remain byte-identical.
- [ ] Add RED undo-storage test: recovery A and current B restore A to current while preserving B through normal protected-current semantics.
- [ ] Run `make test-save`; expected RED.
- [ ] Implement structural health using the same exact-size, regular-file, full-read rules as `flip_save_load()`.
- [ ] Implement staged recovery copy with full write, `fflush()`, `fsync()`, close, size verification and rename before changing current `.sav`.
- [ ] Implement previous->current promotion without destroying `.sav.prev` during the transaction.
- [ ] Implement recovery->current undo through protected-current semantics.
- [ ] Run `make test-save`; expected GREEN.
- [ ] Commit `Add Save Manager recovery primitives`.

### Task 2: State Manager recovery-generation operations

**Files:**
- Modify: `include/flip_state.h`
- Modify: `src/runtime/flip_state.c`
- Modify: `tests/test_flip_state.c`

**Interfaces:**
```c
int flip_state_preserve_recovery(const flip_state_context_t *context);
int flip_state_quarantine_current(const flip_state_context_t *context);
int flip_state_restore_recovery(const flip_state_context_t *context);
```
`flip_state_preserve_recovery()` returns `1` when a current state was copied, `0` when no current state exists, `-1` on error. Recovery suffixes are `.state.recovery` and `.state.recovery.tmp`.

- [ ] Add RED path tests for both recovery-state suffixes.
- [ ] Add RED byte-for-byte preserve test using staged fsynced rename.
- [ ] Add RED no-current-state test requiring stale older `.state.recovery` removal.
- [ ] Add RED quarantine test requiring current `.state` removal only after recovery generation is safely committed.
- [ ] Add RED recovery-state restore test without unserializing in the storage layer.
- [ ] Add RED staging-failure tests proving current state remains recoverable.
- [ ] Run `make test-state`; expected RED.
- [ ] Implement opaque regular/readable state-copy primitives; State Manager owns files, not player policy.
- [ ] Run `make test-state`; expected GREEN.
- [ ] Commit `Add State Manager recovery generation`.

### Task 3: Recovery policy service and persistent resume marker

**Files:**
- Create: `include/flip_recovery.h`
- Create: `src/runtime/flip_recovery.c`
- Create: `tests/test_flip_recovery.c`
- Modify: `Makefile`

**Interfaces:**
```c
typedef struct {
    const char *system_id;
    const char *rom_path;
} flip_recovery_context_t;

void flip_recovery_context_init(
    flip_recovery_context_t *,
    const char *system_id,
    const char *rom_path);

int flip_recovery_resume_blocked(const flip_recovery_context_t *);
int flip_recovery_block_resume(const flip_recovery_context_t *);
int flip_recovery_clear_resume_block(const flip_recovery_context_t *);
```
Test root override: `FLIP_SESSION_RECOVERY_ROOT`, default `/mnt/SDCARD/flipos-dev/sessions`.

- [ ] Add `test-recovery` Makefile target using `/tmp/flipos-recovery-test`.
- [ ] Add RED marker path test for Pokemon Yellow.
- [ ] Add RED absent/present/error marker-state tests; any unsafe read is blocked.
- [ ] Add RED atomic marker creation test using `.restore-pending.tmp`, fsync and rename.
- [ ] Add RED clear-marker success/failure tests.
- [ ] Run `make test-recovery`; expected RED.
- [ ] Implement the marker service with no Save/State file manipulation.
- [ ] Run `make test-recovery`; expected GREEN.
- [ ] Commit `Add recovery resume policy marker`.

### Task 4: Coordinated Restore Previous and Undo transactions

**Files:**
- Modify: `include/flip_recovery.h`
- Modify: `src/runtime/flip_recovery.c`
- Modify: `tests/test_flip_recovery.c`
- Modify: `Makefile`

**Interfaces:**
```c
typedef enum {
    FLIP_RECOVERY_OK = 0,
    FLIP_RECOVERY_NO_PREVIOUS,
    FLIP_RECOVERY_NO_UNDO,
    FLIP_RECOVERY_ERROR
} flip_recovery_result_t;

flip_recovery_result_t flip_recovery_restore_previous(
    flip_core_adapter_t *core,
    const flip_save_context_t *save_context,
    const flip_state_context_t *state_context,
    const flip_recovery_context_t *recovery_context);

flip_recovery_result_t flip_recovery_undo(
    flip_core_adapter_t *core,
    const flip_save_context_t *save_context,
    const flip_state_context_t *state_context,
    const flip_recovery_context_t *recovery_context);
```

- [ ] Add RED coordinator test proving Restore Previous order: validate previous -> preserve current save -> preserve/replace matching recovery-state generation -> activate previous save -> block resume -> quarantine current state -> success.
- [ ] Add RED no-current-state test proving stale recovery state is removed.
- [ ] Add RED failure-after-save-promotion test requiring conservative resume blocking and no eligible mismatched current state.
- [ ] Add RED paired Undo test restoring save + matching state and clearing marker only after both succeed.
- [ ] Add RED save-only Undo test: cartridge recovery succeeds, missing/invalid recovery state keeps marker blocked and current state unavailable.
- [ ] Run `make test-recovery`; expected RED.
- [ ] Implement coordinator with explicit `FLIP_RECOVERY:` logging and no game-specific parsing.
- [ ] Run `make test-save test-state test-recovery`; expected GREEN.
- [ ] Commit `Coordinate non-destructive save recovery`.

### Task 5: Resume policy integration and fresh-state re-enable

**Files:**
- Modify: `include/flip_core_host.h`
- Modify: `src/runtime/flip_core_host.c`
- Modify: `src/emulators/flip_gb.c`
- Modify: `src/runtime/flip_session_adapter.c`
- Modify: `tests/test_flip_core_host.c`
- Create/Modify: `tests/test_recovery_architecture.py`
- Modify: `Makefile`

**Interfaces:**
```c
typedef int (*flip_core_host_persistence_complete_fn)(void *userdata);
```
Core Host config gains an optional persistence-complete callback and userdata. `flip_gb` initializes recovery context, suppresses state restore when blocked, and clears the marker only after a fully successful cartridge+state lifecycle commit.

- [ ] Add RED Core Host tests proving completion callback is not called when cartridge commit fails or state save fails.
- [ ] Add RED test proving callback runs exactly once after both succeed.
- [ ] Add RED architecture test requiring recovery policy composition in `flip_gb` while forbidding marker/file I/O there.
- [ ] Add RED resume-availability test requiring marker-aware policy rather than raw `.state` existence.
- [ ] Run focused tests; expected RED.
- [ ] Implement completion hook after the existing cartridge-first/state-second sequence.
- [ ] In `flip_gb`, explicit `--no-resume` OR persistent recovery block sets `restore_state = NULL`; `.sav` load remains normal.
- [ ] Clear `.restore-pending` only from successful persistence completion; clear failure leaves resume blocked.
- [ ] Update GB/GBC session adapter resume availability to consult the recovery marker.
- [ ] Run `make test-core-host test-recovery test-architecture`; expected GREEN.
- [ ] Commit `Block resume across save recovery timelines`.

### Task 6: Core-aware management commands for Save Safety

**Files:**
- Modify: `src/emulators/flip_gb.c`
- Modify: `tests/test_flip_gb_architecture.py`
- Create/Modify: `tests/test_recovery_architecture.py`

**Interfaces:**
`flip-gb` accepts these narrow non-gameplay commands in addition to existing launch syntax:
```text
flip-gb <rom> --save-safety-status
flip-gb <rom> --restore-previous-save
flip-gb <rom> --undo-last-restore
```
The command path initializes Gambatte, loads the ROM so exact battery-memory size is available, executes shared Save/State/Recovery APIs, unloads/deinitializes the core, prints one machine-readable status line, and exits without opening Video/Input/Audio hosts.

Status line format:
```text
FLIP_SAVE_SAFETY: current=<HEALTHY|MISSING|INVALID> previous=<HEALTHY|MISSING|INVALID> recovery=<HEALTHY|MISSING|INVALID> resume=<AVAILABLE|BLOCKED> undo_state=<AVAILABLE|MISSING|INVALID>
```

- [ ] Add RED architecture test requiring all three management flags while continuing to forbid direct `fopen`, `rename`, raw libretro SAVE_RAM access and path construction in `flip_gb.c`.
- [ ] Add RED command-dispatch test or architecture fixture proving management commands do not call `flip_core_host_run()`.
- [ ] Add RED status-format test covering structurally healthy/missing/invalid save candidates and blocked resume state.
- [ ] Add RED restore/undo command-result tests using fake/shared recovery service seams where practical.
- [ ] Run focused tests; expected RED.
- [ ] Implement one helper that initializes core, loads ROM, initializes Save/State/Recovery contexts, executes the selected shared operation, then unloads/deinitializes in all paths.
- [ ] Keep gameplay `flip-gb <rom> [--no-resume]` behavior unchanged.
- [ ] Run `make test-save test-state test-recovery test-architecture` and `make gb`; expected GREEN.
- [ ] Commit `Add core-aware Save Safety commands`.

### Task 7: Generic frontend Save Safety UI and runner delegation

**Files:**
- Modify: `include/flip_session_adapter.h`
- Modify: `src/runtime/flip_session_adapter.c`
- Modify: `include/flip_system.h`
- Modify: `src/runtime/flip_system.c`
- Modify: `src/frontend/flip_frontend.c`
- Create/Modify focused architecture tests under `tests/`

**Interfaces:**
The session adapter exposes command argument builders/capability hooks, not save-file APIs:
```c
int (*supports_save_safety)(void);
int (*build_start_normal_argv)(const char *runner, const char *rom_path, char **argv, size_t argv_count);
int (*build_save_safety_argv)(const char *runner, const char *rom_path, const char *action, char **argv, size_t argv_count);
```
Frontend invokes the registered runner and parses only the `FLIP_SAVE_SAFETY:` result/status contract. It never knows hash algorithms, save roots, state roots, SRAM sizes or recovery filenames.

- [ ] Add RED architecture assertions that frontend contains none of `.sav`, `.state`, `.recovery`, `.restore-pending`, FNV hashing or save/state root paths.
- [ ] Add RED adapter test/architecture checks for Start Normally -> `--no-resume`, status -> `--save-safety-status`, restore -> `--restore-previous-save`, undo -> `--undo-last-restore`.
- [ ] Add RED UI state test/architecture check requiring only `Resume Game`, `Start Normally`, `Save Safety`, `Restore Previous Save`, `Undo Last Restore` and structural status terms.
- [ ] Require explicit confirmation before restore or undo mutation.
- [ ] Run focused/architecture tests; expected RED.
- [ ] Implement compact per-game action UI; keep normal library/GameSwitcher navigation unchanged unless actions are opened.
- [ ] Use runner status output to show `Healthy`, `Missing`, `Invalid`, `Recovery Available`, `Resume Blocked` without claiming semantic validity.
- [ ] On successful Restore Previous, return/launch with Resume unavailable until a fresh state exists.
- [ ] On paired Undo, Resume may reappear only if status reports it available.
- [ ] Run `make test-architecture frontend`; expected GREEN.
- [ ] Commit `Add Save Safety frontend actions`.

### Task 8: Full software gate and compact verification command

**Files:**
- Modify: `Makefile`
- Modify: `docs/ROADMAP.md` after software gate is green.

**Interfaces:**
Add `make verify` covering every existing test target plus `test-recovery`, `all`, and `git diff --check`, with compact success output and detailed failure log in `/tmp/flipos-verify.log`.

- [ ] Implement `verify` target/script and ensure failure returns nonzero.
- [ ] Run `make clean && make verify`; expected `VERIFY: PASS`.
- [ ] Confirm `git status --short` contains only the six preserved diagnostic files.
- [ ] Record exact HEAD and `sha256sum build/flip-gb` candidate.
- [ ] Update roadmap as software-ready; hardware acceptance remains unchecked.
- [ ] Commit `Prepare Phase 5C hardware candidate`.

### Task 9: Real-Miyoo-Flip Phase 5C hardware acceptance

**Files:**
- Modify: `docs/ROADMAP.md` only after acceptance passes.

- [ ] Atomically deploy exact candidate binaries and verify device SHA equals local candidate SHA.
- [ ] Back up Pokemon Yellow `.sav`, `.sav.prev`, `.state` and hashes to WSL before deliberate recovery tests.
- [ ] Through frontend Save Safety, inspect status and trigger Restore Previous Save.
- [ ] Require `.sav.recovery`, matching `.state.recovery` when current state existed, active previous content, and `.restore-pending` presence.
- [ ] Confirm next launch skips old state; Pokemon reaches normal title flow and CONTINUE loads the selected restored progress.
- [ ] Relaunch before fresh state if necessary and prove the persistent marker still blocks old resume.
- [ ] Perform controlled save; require `FLIP_SAVE: SAVE_OK`, `FLIP_STATE: SAVE_OK`, then `FLIP_RECOVERY: RESUME_REENABLED` and marker removal.
- [ ] Relaunch normally and require fresh `RESTORE_CURRENT_OK` from the restored timeline.
- [ ] Test paired Undo and require pre-restore cartridge save and matching state return together.
- [ ] Repeat restore with no current state and prove stale older `.state.recovery` is removed.
- [ ] Test recovery staging failure and verify authoritative save remains recoverable.
- [ ] Regression: START+SELECT, lid suspend/resume, GameSwitcher exit code 20, RK817 exact-signature recovery, 32768->32000 audio and timing near 59.728 remain unchanged.
- [ ] Restore user's intended Pokemon timeline from WSL backup if acceptance mutations left a different timeline active.
- [ ] Update roadmap with exact hardware-tested commit and binary SHA.
- [ ] Run final `make verify`, compact status/HEAD/SHA check, and integrate only after green evidence.
