# Phase 5 State Manager Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move Game Boy save-state storage into a shared, core-neutral State Manager that preserves legacy `.state` compatibility, writes atomically, validates restores, and can fall back to a previous-known-good state.

**Architecture:** Add a small runtime State Manager that owns system/ROM state paths, file transactions, restore validation, and previous-known-good recovery. Keep raw core serialize/unserialize in the Core Adapter and keep lifecycle timing/exit policy in Core Host/Lifecycle; `flip_gb.c` becomes composition only.

**Tech Stack:** C11, existing FlipOS Core Adapter API, POSIX file I/O, FAT32/vfat SD storage, native host tests, ARM64 static cross-build.

**Spec:** `docs/superpowers/specs/2026-08-26-phase5-state-manager-design.md`

## Global Constraints

- SD-card-only development.
- No internal NAND/MTD writes.
- Preserve the stock Miyoo recovery path.
- Keep `main` known-good and hardware-tested.
- Work on `phase5/state-manager` until hardware acceptance passes.
- Preserve `/mnt/SDCARD/flipos-dev/states/gb/<fnv1a64-rom-path>.state`.
- Preserve the raw Gambatte serialized payload format.
- Preserve Lifecycle Host save semantics and emulator exit code 20.
- Do not redesign GameSwitcher in this phase.
- Do not add another emulator core.
- Battery-backed `.sav`/SRAM protection is out of scope for Phase 5A.

---

### Task 1: State Manager contract, identity, and legacy-compatible paths

**Files:**
- Create: `include/flip_state.h`
- Create: `src/runtime/flip_state.c`
- Create: `tests/test_flip_state.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `flip_core_adapter_t` from `include/flip_core.h`.
- Produces:

```c
typedef struct {
    const char *system_id;
    const char *rom_path;
    int current_was_restored;
} flip_state_context_t;

typedef enum {
    FLIP_STATE_RESTORE_NONE = 0,
    FLIP_STATE_RESTORE_CURRENT = 1,
    FLIP_STATE_RESTORE_PREVIOUS = 2,
    FLIP_STATE_RESTORE_UNUSABLE = 3,
    FLIP_STATE_RESTORE_ERROR = 4
} flip_state_restore_result_t;

void flip_state_context_init(
    flip_state_context_t *context,
    const char *system_id,
    const char *rom_path);

int flip_state_save(
    flip_core_adapter_t *core,
    flip_state_context_t *context);

flip_state_restore_result_t flip_state_restore(
    flip_core_adapter_t *core,
    flip_state_context_t *context);
```

- [ ] **Step 1: Write RED path/identity tests**

Add tests proving FNV-1a compatibility for a fixed ROM path and exact GB path construction. The test must expect the same `<hash>.state` naming currently implemented in `flip_gb.c`.

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
make test-state
```

Expected: build failure because `flip_state.h` / `flip_state.c` do not exist.

- [ ] **Step 3: Implement the minimal public contract and path helpers**

Implement 64-bit FNV-1a over the ROM path, map `system_id="gb"` to `/mnt/SDCARD/flipos-dev/states/gb`, and generate `.state`, `.state.tmp`, `.state.prev`, and `.state.prev.tmp` sibling paths. Keep path helpers internal unless tests require a test-only override.

For host tests, support a compile-time root override:

```c
#ifndef FLIP_STATE_ROOT
#define FLIP_STATE_ROOT "/mnt/SDCARD/flipos-dev/states"
#endif
```

The production build uses the default; tests compile with a temporary directory root.

- [ ] **Step 4: Run path tests GREEN**

Run:

```bash
make test-state
```

Expected: path/identity tests pass with `-Werror`.

- [ ] **Step 5: Commit**

```bash
git add -- include/flip_state.h src/runtime/flip_state.c tests/test_flip_state.c Makefile
git commit -m "Add State Manager path contract"
```

---

### Task 2: Restore validation and previous-known-good fallback

**Files:**
- Modify: `src/runtime/flip_state.c`
- Modify: `tests/test_flip_state.c`

**Interfaces:**
- Uses `flip_core_serialize_size()` and `flip_core_unserialize()`.
- Returns one of the `flip_state_restore_result_t` values defined in Task 1.

- [ ] **Step 1: Write RED restore tests**

Use a fake core adapter with configurable serialize size and unserialize accept/reject behavior. Cover:

```text
no files -> FLIP_STATE_RESTORE_NONE
valid current -> FLIP_STATE_RESTORE_CURRENT
current wrong size + no prev -> FLIP_STATE_RESTORE_UNUSABLE
current unserialize reject + no prev -> FLIP_STATE_RESTORE_UNUSABLE
invalid current + valid prev -> FLIP_STATE_RESTORE_PREVIOUS
invalid current + invalid prev -> FLIP_STATE_RESTORE_UNUSABLE
I/O failure -> FLIP_STATE_RESTORE_ERROR
```

Also assert that successful current restore sets `context.current_was_restored = 1`, while previous restore leaves it `0` because current was not proven good.

- [ ] **Step 2: Run tests and verify RED**

```bash
make test-state
```

Expected: restore cases fail because restore logic is not implemented.

- [ ] **Step 3: Implement candidate loading and validation**

For each candidate:

1. `stat`/open it;
2. require exact size equal to `flip_core_serialize_size(core)`;
3. read the full payload;
4. call `flip_core_unserialize()`;
5. accept only on success.

Restore order must be current, then previous. `.tmp` files are never candidates. Log with stable `FLIP_STATE:` prefixes.

- [ ] **Step 4: Run tests GREEN**

```bash
make test-state
```

Expected: all restore tests pass.

- [ ] **Step 5: Commit**

```bash
git add -- src/runtime/flip_state.c tests/test_flip_state.c
git commit -m "Add State Manager restore recovery"
```

---

### Task 3: Atomic save transaction and known-good preservation

**Files:**
- Modify: `src/runtime/flip_state.c`
- Modify: `tests/test_flip_state.c`

**Interfaces:**
- Uses `flip_core_serialize_size()` and `flip_core_serialize()`.
- Preserves `.state.prev` only from a current state proven by a successful restore in the running context.

- [ ] **Step 1: Write RED save-transaction tests**

Cover these exact behaviors:

```text
serialize size 0 -> save failure
serialize reject -> save failure
successful save -> .state contains exact serialized bytes and .tmp is absent
existing current + context.current_was_restored=1 -> old current copied to .prev before new current commit
existing current + context.current_was_restored=0 -> .prev remains unchanged
failed new write/rename -> previous committed state remains intact
stale .tmp is never promoted by restore
```

Use a test root on a normal host filesystem; inject failures through small internal file-operation hooks compiled only for tests if direct filesystem fault injection is impractical. Keep production behavior hook-free.

- [ ] **Step 2: Run tests and verify RED**

```bash
make test-state
```

Expected: save transaction cases fail.

- [ ] **Step 3: Implement atomic primary write**

Required production sequence:

```text
serialize to memory
ensure state directories
write .state.tmp
fflush
fsync(file)
close
verify expected temp size
rename .state.tmp -> .state
```

Never open `.state` for direct truncating writes.

- [ ] **Step 4: Implement previous-known-good copy**

When `context.current_was_restored == 1` and current `.state` exists:

```text
copy current -> .state.prev.tmp
fflush
fsync(file)
close
rename .state.prev.tmp -> .state.prev
```

Only after that preservation succeeds should the new primary transaction proceed. Do not overwrite `.prev` from an unverified current file.

After a successful save, set `context.current_was_restored = 0`; the new state is committed but has not yet been proven restorable.

- [ ] **Step 5: Run tests GREEN**

```bash
make test-state
```

Expected: all save/recovery transaction tests pass with `-Werror`.

- [ ] **Step 6: Commit**

```bash
git add -- src/runtime/flip_state.c tests/test_flip_state.c
git commit -m "Add atomic State Manager saves"
```

---

### Task 4: Migrate `flip-gb` to shared State Manager

**Files:**
- Modify: `src/emulators/flip_gb.c`
- Modify: `Makefile`
- Create: `tests/test_state_manager_architecture.py`

**Interfaces:**
- `flip_gb.c` creates one `flip_state_context_t` with `system_id="gb"` and `rom_path=argv[1]`.
- Core Host keeps the existing callback signatures. Add thin bridge callbacks in `flip_gb.c` only if Core Host callback types do not directly match State Manager signatures.

- [ ] **Step 1: Write RED architecture test**

Assert `flip_gb.c` no longer contains:

```text
state_path_hash
ensure_state_directory
fopen(
fsync(
rename(
GB_STATE_DIR
```

Assert it does contain `flip_state_context_t`, `flip_state_save`, and `flip_state_restore` (directly or through named bridge callbacks).

- [ ] **Step 2: Run architecture test and verify RED**

```bash
python3 tests/test_state_manager_architecture.py
```

Expected: FAIL because legacy file handling remains embedded in `flip_gb.c`.

- [ ] **Step 3: Replace embedded persistence with State Manager composition**

Keep only:

```text
argv validation
Gambatte adapter selection
state context construction
Core Host config
run and return exit code
```

Map State Manager restore results to Core Host's existing restore callback convention:

```text
CURRENT/PREVIOUS -> 1
NONE -> 0
UNUSABLE/ERROR -> -1
```

Do not alter exit code 20 or Lifecycle Host behavior.

- [ ] **Step 4: Wire build and tests**

Add `flip_state.o` to the GB/runtime objects, add `test-state`, and include `tests/test_state_manager_architecture.py` in `test-architecture`.

- [ ] **Step 5: Run focused regression GREEN**

```bash
make test-state
make test-core-host
make test-lifecycle
make test-architecture
make all
```

Expected: all pass; only the already-known external Gambatte/static glibc warnings are acceptable.

- [ ] **Step 6: Commit**

```bash
git add -- src/emulators/flip_gb.c src/runtime/flip_state.c include/flip_state.h tests/test_flip_state.c tests/test_state_manager_architecture.py Makefile
git commit -m "Route GB states through State Manager"
```

---

### Task 5: Full software verification and exact-source checkpoint

**Files:**
- No production changes unless verification exposes a defect.
- Modify after successful hardware acceptance: `docs/ROADMAP.md`

- [ ] **Step 1: Run the complete regression suite**

```bash
make clean
make test-controls
make test-timing
make test-audio-resampler
make test-audio
make test-lifecycle
make test-core
make test-core-host
make test-state
make test-perf-diag
make test-telemetry
make test-architecture
make all
git diff --check
```

Expected: all tests/builds pass; no whitespace errors.

- [ ] **Step 2: Record exact binary and branch state**

```bash
git log --oneline -5
git status --short
sha256sum build/flip-gb
```

Expected: only the user's pre-existing diagnostic logs may remain untracked; no uncommitted source changes.

- [ ] **Step 3: Commit any verification-only fixes before hardware deployment**

If tests exposed a defect, fix it TDD-first, rerun the full suite, and commit exact files. Do not hardware-test uncommitted production source.

---

### Task 6: Real-Miyoo-Flip hardware acceptance

**Files:**
- Modify after all hardware checks pass: `docs/ROADMAP.md`

- [ ] **Step 1: Deploy SD-only atomically**

```bash
adb push build/flip-gb /mnt/SDCARD/flipos-dev/bin/flip-gb.new
adb shell 'chmod +x /mnt/SDCARD/flipos-dev/bin/flip-gb.new && mv -f /mnt/SDCARD/flipos-dev/bin/flip-gb.new /mnt/SDCARD/flipos-dev/bin/flip-gb'
```

Compare local/device SHA-256 before testing.

- [ ] **Step 2: Verify legacy state compatibility**

Launch a GB title with an existing pre-Phase-5 `.state`. Require successful resume without migration/rewrite-on-load.

- [ ] **Step 3: Verify normal save/resume paths**

Exercise:

```text
START+SELECT normal exit
relaunch/resume
lid suspend/resume
GameSwitcher save/exit-code-20 path
```

Require existing lifecycle behavior to remain unchanged.

- [ ] **Step 4: Establish a previous-known-good state**

Start from a successfully restored current state, play briefly, trigger a successful save, then confirm on SD that both `.state` and `.state.prev` exist and sizes match the core serialize size.

- [ ] **Step 5: Controlled corruption fallback test**

Back up the SD state files outside the active path first. Corrupt only the current `.state` on the development SD (for example truncate it), leave `.state.prev` intact, relaunch, and require:

```text
FLIP_STATE: CURRENT_INVALID ...
FLIP_STATE: RESTORE_PREVIOUS_OK ...
```

The game must resume from the previous-known-good state. Restore the test files afterward as appropriate.

- [ ] **Step 6: Verify failed-current handling does not touch NAND/MTD**

All corruption/recovery manipulation must remain under `/mnt/SDCARD/flipos-dev/states/...` only.

- [ ] **Step 7: Recheck audio/timing regression gates**

While the resumed game runs, require:

```text
ALSA hw_params rate: 32000
correct audio pitch/speed/sync
RK817 exact-signature recovery still works across lid suspend/resume
FPS remains around the verified 59.7 target
no renewed 10-19 ms audio write backlog
```

- [ ] **Step 8: Update roadmap only after acceptance**

Mark Phase 5A State Manager items complete and record the exact hardware-tested code commit and binary hash. Keep Phase 5B Save Data Protection and Phase 5C Recovery UI/tool pending.

- [ ] **Step 9: Final verification before promotion**

Rerun the complete software suite on the exact documented head, compare `phase5/state-manager` against `main`, require `behind_by=0`, and promote only by fast-forward after hardware acceptance.
