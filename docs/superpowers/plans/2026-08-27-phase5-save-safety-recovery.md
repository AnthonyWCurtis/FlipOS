# Phase 5C Save Safety / Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add non-destructive cartridge-save recovery, paired save/state undo, persistent resume blocking after a timeline restore, and minimal frontend recovery actions on top of the proven Phase 5B Save/State Managers.

**Architecture:** Keep storage mechanics in Save Manager and State Manager, and add a focused recovery/session-policy layer that coordinates cross-file timeline switches and the persistent per-ROM `.restore-pending` marker. `flip-gb` remains a composition layer; Core Host clears the marker only after both cartridge save and fresh state commit succeed, while the frontend exposes Resume Game, Start Normally, Save Safety, Restore Previous Save, and Undo Last Restore without parsing game-specific save contents.

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

### Task 1: Save Manager recovery primitives and health inspection

**Files:**
- Modify: `include/flip_save.h`
- Modify: `src/runtime/flip_save.c`
- Modify: `tests/test_flip_save.c`

**Interfaces:**
- Produces: `flip_save_health_t flip_save_health(flip_core_adapter_t *core, const flip_save_context_t *context, const char *suffix)`.
- Produces: `int flip_save_restore_previous(flip_core_adapter_t *core, const flip_save_context_t *context)`.
- Produces: `int flip_save_restore_recovery(flip_core_adapter_t *core, const flip_save_context_t *context)`.
- Recovery suffixes: `.sav.recovery` and `.sav.recovery.tmp`.

- [ ] Add RED path tests for `.sav.recovery` and `.sav.recovery.tmp` using Pokemon Yellow's frozen identity `792f9628ba43b7d9`.
- [ ] Add RED health tests covering missing, exact-size/readable, wrong-size, non-regular and read-failure candidates.
- [ ] Add RED restore-previous test: current `.sav` A and `.sav.prev` B must finish with `.sav.recovery` A, `.sav` B, `.sav.prev` still B, and no authoritative `.tmp` files.
- [ ] Add RED failure test forcing `.sav.recovery.tmp` creation failure and assert `.sav`/`.sav.prev` remain byte-identical.
- [ ] Add RED undo-storage test: `.sav.recovery` A and current B must restore A to `.sav` while preserving B through normal protected-current semantics.
- [ ] Run `make test-save`; expected RED because recovery APIs do not exist.
- [ ] Implement structural health using the same exact-size, regular-file, full-read rules already used by `flip_save_load()`.
- [ ] Implement staged recovery copy with full write, `fflush()`, `fsync()`, close, size verification and rename before changing current `.sav`.
- [ ] Implement previous->current promotion without destroying `.sav.prev` during the recovery transaction.
- [ ] Implement recovery->current undo using protected-current semantics so the timeline being left remains recoverable.
- [ ] Run `make test-save`; expected GREEN.
- [ ] Commit `Add Save Manager recovery primitives`.

### Task 2: State Manager recovery-generation operations

**Files:**
- Modify: `include/flip_state.h`
- Modify: `src/runtime/flip_state.c`
- Modify: `tests/test_flip_state.c`

**Interfaces:**
- Produces: `int flip_state_preserve_recovery(const flip_state_context_t *context)` returning `1` when current state was copied, `0` when no current state exists, `-1` on error.
- Produces: `int flip_state_quarantine_current(const flip_state_context_t *context)`.
- Produces: `int flip_state_restore_recovery(const flip_state_context_t *context)` returning `1` when restored, `0` when no valid recovery state is available, `-1` on I/O error.
- Recovery suffixes: `.state.recovery` and `.state.recovery.tmp`.

- [ ] Add RED path tests for `.state.recovery` and `.state.recovery.tmp`.
- [ ] Add RED test that current `.state` is copied byte-for-byte to `.state.recovery` through a staged fsynced rename.
- [ ] Add RED test that no current state removes any stale older `.state.recovery`, proving a new recovery generation cannot inherit an old state.
- [ ] Add RED test that quarantine removes current `.state` only after its recovery copy exists.
- [ ] Add RED restore test that `.state.recovery` is atomically promoted back to `.state` without unserializing it in the storage layer.
- [ ] Add RED failure tests proving failed staging leaves current state available and never promotes a partial temporary file.
- [ ] Run `make test-state`; expected RED.
- [ ] Implement opaque regular/readable state-copy primitives; keep State Manager responsible for files, not resume policy.
- [ ] Run `make test-state`; expected GREEN.
- [ ] Commit `Add State Manager recovery generation`.

### Task 3: Recovery policy service and persistent resume marker

**Files:**
- Create: `include/flip_recovery.h`
- Create: `src/runtime/flip_recovery.c`
- Create: `tests/test_flip_recovery.c`
- Modify: `Makefile`

**Interfaces:**
- Context:
```c
typedef struct {
    const char *system_id;
    const char *rom_path;
} flip_recovery_context_t;
```
- Produces:
```c
void flip_recovery_context_init(flip_recovery_context_t *, const char *system_id, const char *rom_path);
int flip_recovery_resume_blocked(const flip_recovery_context_t *);
int flip_recovery_block_resume(const flip_recovery_context_t *);
int flip_recovery_clear_resume_block(const flip_recovery_context_t *);
```
- Test root override: `FLIP_SESSION_RECOVERY_ROOT`, default `/mnt/SDCARD/flipos-dev/sessions`.

- [ ] Add Makefile `test-recovery` target with `-DFLIP_SESSION_RECOVERY_ROOT='"/tmp/flipos-recovery-test"'`.
- [ ] Add RED path test freezing Pokemon Yellow marker path `/tmp/flipos-recovery-test/gb/792f9628ba43b7d9.restore-pending`.
- [ ] Add RED tests: absent marker => resume allowed; present regular marker => blocked; non-regular/unreadable marker state => blocked conservatively.
- [ ] Add RED atomic-create test requiring zero-length `.restore-pending.tmp`, `fflush()`/`fsync()`, rename and no stale tmp.
- [ ] Add RED clear-marker test and a forced clear failure case.
- [ ] Run `make test-recovery`; expected RED.
- [ ] Implement the marker service with no Save/State file manipulation.
- [ ] Run `make test-recovery`; expected GREEN.
- [ ] Commit `Add recovery resume policy marker`.

### Task 4: Coordinated Restore Previous and Undo Last Restore transactions

**Files:**
- Modify: `include/flip_recovery.h`
- Modify: `src/runtime/flip_recovery.c`
- Modify: `tests/test_flip_recovery.c`
- Modify: `Makefile` test link inputs.

**Interfaces:**
- Produces:
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

- [ ] Add RED coordinator test proving Restore Previous order: validate previous -> preserve current `.sav` -> preserve/replace matching `.state.recovery` generation -> activate `.sav.prev` -> block resume -> quarantine current `.state` -> success.
- [ ] Add RED test where no current `.state` exists and assert stale `.state.recovery` is removed before success.
- [ ] Add RED post-save-promotion failure test and require conservative outcome: marker/marker-read state leaves resume blocked and current `.state` is not eligible for automatic resume.
- [ ] Add RED Undo paired test restoring `.sav.recovery` + `.state.recovery` and clearing marker only after both succeed.
- [ ] Add RED Undo save-only test: valid `.sav.recovery` plus missing/invalid `.state.recovery` restores cartridge save but marker remains blocked and current `.state` is absent/quarantined.
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
- Modify/Create: `tests/test_recovery_architecture.py`
- Modify: `Makefile`

**Interfaces:**
- Core Host gets an optional callback invoked only after `commit_save` and `save_state` both succeed:
```c
typedef int (*flip_core_host_persistence_complete_fn)(void *userdata);
```
- `flip_gb` initializes `flip_recovery_context_t`, suppresses state restore when marker says blocked, and supplies a persistence-complete callback that clears the marker.
- `flip_session_adapter` resume availability must return false while marker is present/error, even if `.state` exists.

- [ ] Add RED Core Host test proving completion callback is not called if cartridge commit fails.
- [ ] Add RED Core Host test proving completion callback is not called if state save fails.
- [ ] Add RED Core Host test proving callback runs exactly once after both succeed.
- [ ] Add RED architecture test requiring recovery policy in `flip_gb` composition while forbidding marker/file I/O there.
- [ ] Add RED resume-availability test/architecture rule requiring marker-aware policy rather than raw `.state` existence alone.
- [ ] Run focused tests; expected RED.
- [ ] Implement Core Host completion hook after the existing cartridge-first/state-second save sequence.
- [ ] In `flip_gb`, treat explicit `--no-resume` OR persistent recovery block as `restore_state = NULL`; always load `.sav`.
- [ ] Clear `.restore-pending` only from the successful persistence-complete hook; if clear fails, leave resume blocked.
- [ ] Update GB/GBC session adapter resume availability to consult the recovery marker before advertising resume.
- [ ] Run `make test-core-host test-recovery test-architecture`; expected GREEN.
- [ ] Commit `Block resume across save recovery timelines`.

### Task 6: Minimal frontend Save Safety actions

**Files:**
- Modify: `include/flip_system.h`
- Modify: `src/runtime/flip_system.c`
- Modify: `include/flip_session_adapter.h`
- Modify: `src/runtime/flip_session_adapter.c`
- Modify: `src/frontend/flip_frontend.c`
- Modify/Create focused architecture tests under `tests/`.

**Interfaces:**
- Extend the system/session adapter with system-owned recovery capabilities so frontend does not hard-code GB paths:
```c
int (*start_normal_available)(const char *rom_path);
int (*save_safety_status)(const char *rom_path, flip_save_safety_status_t *status);
int (*restore_previous_save)(const char *rom_path);
int (*undo_last_restore)(const char *rom_path);
```
- Frontend presents `Resume Game`, `Start Normally`, `Save Safety`; Save Safety shows structural status only and actions `Restore Previous Save`, `Undo Last Restore` when applicable.

- [ ] Add RED architecture assertions that frontend does not construct `.sav`, `.state`, `.recovery`, hash or marker paths directly.
- [ ] Add RED adapter tests/architecture assertions for launch arguments: Resume uses normal runner arguments; Start Normally appends `--no-resume`.
- [ ] Add RED status tests ensuring UI terminology does not claim semantic game-save validity: `Healthy`, `Missing`, `Invalid`, `Recovery Available`, `Resume Blocked` only.
- [ ] Add RED confirmation-flow assertion: restore/undo require an explicit user confirmation before mutation.
- [ ] Run architecture/focused tests; expected RED.
- [ ] Implement adapter-facing GB/GBC recovery operations using the shared managers/policy service.
- [ ] Add a compact per-game action screen in frontend; keep the main library/GameSwitcher flow unchanged unless the user opens actions.
- [ ] On successful Restore Previous, show confirmation and launch/return with Resume unavailable until a fresh state exists.
- [ ] On Undo paired restore, permit Resume only when the marker was successfully cleared by the transaction.
- [ ] Run `make test-architecture` and build frontend; expected GREEN.
- [ ] Commit `Add Save Safety frontend actions`.

### Task 7: Full software gate and compact verification command

**Files:**
- Modify: `Makefile`
- Modify: `docs/ROADMAP.md` only after software gate is green.

**Interfaces:**
- Add `make verify` so normal success output is compact; on failure preserve detailed test output in `/tmp/flipos-verify.log` or print a short tail.

- [ ] Add RED/shell-level check or Makefile target implementation for `verify` covering controls, timing, audio, audio-resampler, lifecycle, core, core-host, state, save, recovery, perf-diag, telemetry, architecture, `all`, and `git diff --check`.
- [ ] Run `make clean && make verify`; expected `VERIFY: PASS` when green.
- [ ] Confirm `git status --short` contains only the six preserved diagnostic files.
- [ ] Record exact `git rev-parse HEAD` and `sha256sum build/flip-gb` candidate.
- [ ] Update roadmap Phase 5C as software-ready but leave hardware acceptance incomplete.
- [ ] Commit `Prepare Phase 5C hardware candidate`.

### Task 8: Real-Miyoo-Flip Phase 5C hardware acceptance

**Files:**
- Modify: `docs/ROADMAP.md` after all acceptance checks pass.

- [ ] Atomically deploy exact candidate binaries to SD and verify device SHA equals local candidate SHA.
- [ ] Back up Pokemon Yellow current `.sav`, `.sav.prev`, `.state` and hashes to WSL before destructive recovery testing.
- [ ] Ensure current and previous cartridge saves represent distinguishable progress where practical; never infer this from hashes alone without a visual check.
- [ ] Trigger `Restore Previous Save` through the user-facing path; require `.sav.recovery` and matching `.state.recovery` preservation and `.restore-pending` presence.
- [ ] Confirm active `.sav` equals selected protected previous content and internal NAND/MTD was untouched.
- [ ] Relaunch and require no `FLIP_STATE: RESTORE_*` from the quarantined old state; Pokemon must reach normal title flow and CONTINUE must load the selected restored progress.
- [ ] Exit before creating a valid fresh state if needed, relaunch, and prove marker persistence still blocks the old resume state.
- [ ] Perform a controlled lifecycle save; require `FLIP_SAVE: SAVE_OK`, `FLIP_STATE: SAVE_OK`, then `FLIP_RECOVERY: RESUME_REENABLED` and marker removal.
- [ ] Relaunch normally and require fresh `FLIP_STATE: RESTORE_CURRENT_OK` from the restored timeline.
- [ ] Test `Undo Last Restore` with valid paired recovery; require pre-restore cartridge save and matching state return together and normal Resume becomes available.
- [ ] Repeat recovery with no current state to prove a stale older `.state.recovery` cannot survive into the newer recovery generation.
- [ ] Test failure injection on a recovery staging path and verify authoritative save remains recoverable.
- [ ] Regression: START+SELECT, lid suspend/resume, GameSwitcher exit code 20, RK817 exact-signature deep-resume recovery, 32768->32000 audio and timing near 59.728 remain unchanged.
- [ ] Restore the user's known-good Pokemon backup after deliberate acceptance mutations if the final accepted timeline is not the desired one.
- [ ] Update roadmap with exact hardware-tested commit and binary SHA.
- [ ] Run final `make verify`, `git diff --check`, compact status/HEAD/SHA check, then integrate to `main` only after green evidence.
