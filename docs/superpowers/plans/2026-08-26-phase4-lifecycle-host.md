# Phase 4A5 Lifecycle Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move save-before-leave, exit-code, lifecycle checkpoint, shared cleanup, and resume-timing policy out of `flip-gb` into a reusable FlipOS Lifecycle Host while preserving current hardware behavior.

**Architecture:** `flip_lifecycle` consumes logical `flip_system_actions_t` values and runs policy at two explicit frame checkpoints. `flip-gb` supplies only its existing core-specific save callback; Lifecycle Host owns exit policy, shared host cleanup order, and Timing Host resync for restored sessions.

**Tech Stack:** C11, native GCC host tests, AArch64 cross build, existing FlipOS Video/Input/Audio/Timing/Settings hosts, Gambatte libretro.

**Spec:** `docs/superpowers/specs/2026-08-26-phase4-lifecycle-host-design.md`

## Global Constraints

- Preserve frontend/GameSwitcher exit code `20` exactly.
- Preserve START+SELECT handling after FPS accounting and before frame wait.
- Preserve suspend/GameSwitcher save-success requirement before exit code `20`.
- Do not move Gambatte serialization or state-file storage into Lifecycle Host.
- Do not change save-state format/path/hash behavior.
- Do not redesign frontend session metadata or deep-suspend behavior.
- No direct NAND/MTD access.
- `main` remains the hardware-tested baseline until physical acceptance passes.

---

### Task 1: Lifecycle policy API and native tests

**Files:**
- Create: `include/flip_lifecycle.h`
- Create: `src/runtime/flip_lifecycle.c`
- Create: `tests/test_flip_lifecycle.c`

**Interfaces:**
- Consumes: `flip_system_actions_t`, `flip_timing_t`, shared host close/flush functions.
- Produces: `flip_lifecycle_handle`, `flip_lifecycle_resume_timing`, `flip_lifecycle_shutdown_shared`, `FLIP_LIFECYCLE_EXIT_SWITCHER`.

- [ ] **Step 1: Write the failing lifecycle test**

Create `tests/test_flip_lifecycle.c` with save callback counters and link-time stubs for shared cleanup functions. Test these exact behaviors:

```c
flip_system_actions_t actions = {0};
flip_lifecycle_result_t result;

assert(flip_lifecycle_handle(
    &actions,
    FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
    save_ok,
    NULL,
    &result) == 0);
assert(result.should_exit == 0);
```

Add separate assertions for lid transition propagation, suspend success/failure, simultaneous failed suspend + successful GameSwitcher, GameSwitcher success, deferred normal exit success/failure, checkpoint filtering, timing resync, and shared cleanup order `settings -> audio -> input -> video`.

- [ ] **Step 2: Run RED verification**

Run:

```bash
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
  tests/test_flip_lifecycle.c \
  src/runtime/flip_lifecycle.c src/runtime/flip_timing.c \
  -o /tmp/test-flip-lifecycle
```

Expected before production files exist: compile failure for missing `flip_lifecycle.h` and/or `flip_lifecycle.c`.

- [ ] **Step 3: Implement the public API**

Create `include/flip_lifecycle.h` with:

```c
#ifndef FLIP_LIFECYCLE_H
#define FLIP_LIFECYCLE_H

#include "flip_controls.h"
#include "flip_timing.h"

typedef enum {
    FLIP_LIFECYCLE_REASON_NONE = 0,
    FLIP_LIFECYCLE_REASON_SUSPEND,
    FLIP_LIFECYCLE_REASON_GAMESWITCHER,
    FLIP_LIFECYCLE_REASON_EXIT
} flip_lifecycle_reason_t;

typedef enum {
    FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE = 0,
    FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME
} flip_lifecycle_checkpoint_t;

typedef int (*flip_lifecycle_save_fn)(void *userdata);

typedef struct {
    flip_lifecycle_reason_t reason;
    int lid_changed;
    int lid_closed;
    int save_attempted;
    int save_succeeded;
    int should_exit;
    int exit_code;
} flip_lifecycle_result_t;

#define FLIP_LIFECYCLE_EXIT_SWITCHER 20

int flip_lifecycle_handle(
    const flip_system_actions_t *actions,
    flip_lifecycle_checkpoint_t checkpoint,
    flip_lifecycle_save_fn save_fn,
    void *userdata,
    flip_lifecycle_result_t *result);

int flip_lifecycle_resume_timing(
    flip_timing_t *timing);

int flip_lifecycle_shutdown_shared(void);

#endif
```

- [ ] **Step 4: Implement checkpoint policy**

In `src/runtime/flip_lifecycle.c`:

- zero `result` on entry;
- reject null `actions`/`result` and invalid checkpoint with `-1`;
- always propagate `lid_changed`/`lid_closed`;
- immediate checkpoint handles suspend then GameSwitcher;
- each immediate action invokes `save_fn` when present;
- successful suspend/GameSwitcher sets `should_exit=1`, `exit_code=20`;
- failed suspend continues to a simultaneous GameSwitcher request;
- after-frame checkpoint handles only `exit_requested`;
- normal exit invokes save, records success/failure, and sets `should_exit=1`, `exit_code=0` regardless of save result;
- return `1` when the checkpoint observed a lifecycle action, `0` when it did not, `-1` for invalid input.

- [ ] **Step 5: Implement timing resume and shared cleanup**

`flip_lifecycle_resume_timing()` must return `flip_timing_resync(timing)`.

`flip_lifecycle_shutdown_shared()` must execute this order even when settings flush fails:

```c
int settings_result = flip_settings_flush();
flip_audio_close();
flip_input_close();
flip_video_close();
return settings_result < 0 ? -1 : 0;
```

- [ ] **Step 6: Run GREEN verification**

Run:

```bash
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
  tests/test_flip_lifecycle.c \
  src/runtime/flip_lifecycle.c src/runtime/flip_timing.c \
  -o /tmp/test-flip-lifecycle && \
/tmp/test-flip-lifecycle
```

Expected:

```text
flip_lifecycle tests passed
```

- [ ] **Step 7: Commit Task 1**

```bash
git add include/flip_lifecycle.h src/runtime/flip_lifecycle.c tests/test_flip_lifecycle.c
git commit -m "Add Lifecycle Host policy"
```

---

### Task 2: Build-system integration

**Files:**
- Modify: `Makefile`

**Interfaces:**
- Consumes: Lifecycle Host source/header/test from Task 1.
- Produces: `make test-lifecycle` and ARM `flip_lifecycle.o` linked into `flip-gb`.

- [ ] **Step 1: Add Lifecycle Host to GB objects**

Add:

```make
$(BUILD)/flip_lifecycle.o
```

to `GB_OBJS` after `flip_timing.o`.

Do not add it to the frontend yet; frontend behavior remains unchanged in 4A5.

- [ ] **Step 2: Add the native test target**

Add `test-lifecycle` to `.PHONY` and:

```make
test-lifecycle: $(BUILD)/test-flip-lifecycle
	$(BUILD)/test-flip-lifecycle

$(BUILD)/test-flip-lifecycle: tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_timing.c include/flip_lifecycle.h | $(BUILD)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude \
		tests/test_flip_lifecycle.c src/runtime/flip_lifecycle.c src/runtime/flip_timing.c \
		-o $@
```

The test file provides stubs for `flip_settings_flush`, `flip_audio_close`, `flip_input_close`, and `flip_video_close`.

- [ ] **Step 3: Add ARM object rule and clean target**

Add:

```make
$(BUILD)/flip_lifecycle.o: src/runtime/flip_lifecycle.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
```

and remove `$(BUILD)/test-flip-lifecycle` in `make clean`.

- [ ] **Step 4: Verify native tests**

Run:

```bash
make test-controls
make test-timing
make test-audio
make test-lifecycle
```

Expected all four test suites to pass.

- [ ] **Step 5: Commit Task 2**

```bash
git add Makefile
git commit -m "Build and test Lifecycle Host"
```

---

### Task 3: Guarded Flip_GB lifecycle migration

**Files:**
- Modify: `src/emulators/flip_gb.c`
- Create temporarily: `tools/apply_phase4a5_lifecycle_host.py`

**Interfaces:**
- Consumes: Lifecycle Host API.
- Produces: Flip_GB with lifecycle policy delegated to the shared host while retaining core serialization.

- [ ] **Step 1: Add a core-specific save callback**

Add `#include "flip_lifecycle.h"` after the timing include.

Remove local `#define FLIPGB_EXIT_SWITCHER 20`.

Add after `save_resume_state()`:

```c
static int lifecycle_save_cb(void *userdata)
{
    (void)userdata;
    return save_resume_state();
}
```

- [ ] **Step 2: Preserve restore state result for timing resync**

Keep `resume_result` in scope through Timing Host initialization.

After successful `flip_timing_init()`:

```c
if (resume_result > 0) {
    if (flip_lifecycle_resume_timing(&timing) < 0) {
        fprintf(stderr, "FLIPGB: lifecycle timing resync failed\n");
    }
}
```

- [ ] **Step 3: Replace immediate suspend/GameSwitcher policy**

Immediately after `flip_audio_end_frame()` call:

```c
flip_lifecycle_result_t lifecycle_immediate;

int lifecycle_result = flip_lifecycle_handle(
    &control_state.system,
    FLIP_LIFECYCLE_CHECKPOINT_IMMEDIATE,
    lifecycle_save_cb,
    NULL,
    &lifecycle_immediate);

if (lifecycle_result < 0) {
    fprintf(stderr, "FLIPGB: lifecycle immediate handling failed\n");
    break;
}

if (lifecycle_immediate.should_exit) {
    flipgb_exit_code = lifecycle_immediate.exit_code;
    break;
}
```

Remove the old local lid/suspend and GameSwitcher save/exit blocks. Keep FPS toggle and hotkey handling in their existing positions.

- [ ] **Step 4: Replace START+SELECT policy at the after-frame checkpoint**

After Timing Host FPS accounting and before `flip_timing_wait_frame()`:

```c
flip_lifecycle_result_t lifecycle_after_frame;

lifecycle_result = flip_lifecycle_handle(
    &control_state.system,
    FLIP_LIFECYCLE_CHECKPOINT_AFTER_FRAME,
    lifecycle_save_cb,
    NULL,
    &lifecycle_after_frame);

if (lifecycle_result < 0) {
    fprintf(stderr, "FLIPGB: lifecycle after-frame handling failed\n");
    break;
}

if (lifecycle_after_frame.should_exit) {
    flipgb_exit_code = lifecycle_after_frame.exit_code;
    break;
}
```

Remove the old local START+SELECT save/break block.

- [ ] **Step 5: Move shared shutdown ordering**

At normal teardown preserve:

```c
unload_rom(&game);
retro_deinit();
```

then replace direct audio/input/video closes with:

```c
if (flip_lifecycle_shutdown_shared() < 0) {
    fprintf(stderr, "FLIPGB: shared lifecycle cleanup warning\n");
}
```

Apply the same shared cleanup call in the timing-initialization failure path after core teardown.

- [ ] **Step 6: Use a literal guarded migration helper**

`tools/apply_phase4a5_lifecycle_host.py` must use exact `str.replace` spans only, require exactly one match per replacement, refuse partially migrated source, and verify these old policy tokens are absent after migration:

```text
FLIPGB_EXIT_SWITCHER
FLIPGB_LID suspend requested
FLIPGB_STATE: switcher save failed
FLIPGB: START+SELECT exit
```

It must also verify exactly two `flip_lifecycle_handle(` calls exist.

- [ ] **Step 7: Verify source shape**

Run:

```bash
grep -nE 'FLIPGB_EXIT_SWITCHER|suspend requested|switcher save failed|START\+SELECT exit' src/emulators/flip_gb.c || echo "PASS: local lifecycle policy removed"
grep -n "flip_lifecycle_handle" src/emulators/flip_gb.c
```

Expected: first command prints PASS; second command shows two calls.

---

### Task 4: Full verification and real-Miyoo-Flip gate

**Files:**
- No production source additions beyond Tasks 1–3.
- Update `docs/ROADMAP.md` only after hardware acceptance.

- [ ] **Step 1: Clean build and all host tests**

Run:

```bash
make clean
make test-controls
make test-timing
make test-audio
make test-lifecycle
make all
git diff --check
git status
```

Expected: all host tests pass, ARM64 frontend and `flip-gb` link successfully, and only the intended migrated source remains uncommitted if migration is being hardware-tested before preservation.

- [ ] **Step 2: Deploy exact build**

Copy `build/flip-frontend` and `build/flip-gb` to the development SD, `sync`, and verify SHA-256 hashes match local binaries.

- [ ] **Step 3: Hardware acceptance**

Verify:

```text
GB gameplay normal
MENU -> GameSwitcher -> resume correct
lid close -> save/leave/deep suspend -> lid-open resume correct
audio and input return after resume
START+SELECT saves/exits normally
volume and brightness hotkeys unchanged
FPS remains near target
no catch-up burst after resume
```

Use the existing SD stderr log capture and inspect `FLIP_LIFECYCLE`, `FLIPGB_STATE`, `flip_audio`, and `FLIPGB_FPS` lines.

- [ ] **Step 4: Preserve hardware-tested source**

After physical acceptance:

```bash
git add src/emulators/flip_gb.c
git commit -m "Move emulator lifecycle policy into Lifecycle Host"
git push origin phase4/lifecycle-host
```

- [ ] **Step 5: Mark 4A5 hardware verified and fast-forward main**

Update `docs/ROADMAP.md` with all 4A5 checks and the hardware-tested source commit. Fast-forward `main` only with non-force semantics and verify `main` and `phase4/lifecycle-host` are identical.
