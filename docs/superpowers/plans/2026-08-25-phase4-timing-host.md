# Phase 4A3 Global Timing Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract FlipOS frame pacing, FPS accounting and schedule resynchronisation from Flip_GB into a reusable Timing Host while preserving the hardware-tested Game Boy timing behaviour.

**Architecture:** `Flip_GB` continues to obtain Gambatte's target FPS and applies the GB-specific 59.7275 fallback. A new `flip_timing` runtime module owns `CLOCK_MONOTONIC` deadlines, measured-FPS state, absolute sleeping and late-frame resync. The emulator consumes only measured-FPS/resync results.

**Tech Stack:** C11, POSIX `clock_gettime`, `clock_nanosleep`, ARM64 cross-build via `aarch64-linux-gnu-*`, native GCC unit tests.

**Spec:** `docs/superpowers/specs/2026-08-25-phase4-timing-host-design.md`

## Global Constraints

- SD-card-only development; do not modify NAND/MTD.
- `main` remains the known-good hardware-tested baseline until real-device validation passes.
- Preserve the current 59.7275 FPS Game Boy fallback outside the shared host.
- Preserve absolute `CLOCK_MONOTONIC` pacing and the existing one-frame-late resync threshold.
- No audio redesign, frame skipping, adaptive pacing, vsync redesign, lifecycle move, or performance tuning in Phase 4A3.

---

### Task 1: Add deterministic Timing Host state and tests

**Files:**
- Create: `include/flip_timing.h`
- Create: `src/runtime/flip_timing.c`
- Create: `tests/test_flip_timing.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: POSIX monotonic clock functions.
- Produces: `flip_timing_t`, `flip_timing_init()`, `flip_timing_record_frame()`, `flip_timing_wait_frame()`, `flip_timing_resync()`.

- [ ] **Step 1: Write the failing timing-host test**

Create tests that require the new API and verify invalid-FPS rejection, the 59.7275 FPS frame period, one-second FPS measurement, forced-late resync, and explicit resync state reset.

- [ ] **Step 2: Run the test to verify RED**

Run:

```bash
gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/test_flip_timing.c src/runtime/flip_timing.c -o /tmp/test-flip-timing
```

Expected before implementation: compilation fails because `flip_timing.h` / `flip_timing.c` do not exist.

- [ ] **Step 3: Implement the minimal Timing Host**

Implement the exact API from the design. Use `CLOCK_MONOTONIC`, rounded `1e9 / target_fps` nanoseconds, `TIMER_ABSTIME`, one-second measured-FPS windows, and the existing `next_frame + frame_period` late threshold.

- [ ] **Step 4: Run the native timing tests**

Run:

```bash
make test-timing
```

Expected:

```text
flip_timing tests passed
```

- [ ] **Step 5: Run existing controls tests**

Run:

```bash
make test-controls
```

Expected:

```text
flip_controls tests passed
```

### Task 2: Wire Timing Host into Flip_GB

**Files:**
- Modify: `src/emulators/flip_gb.c`
- Modify: `Makefile`

**Interfaces:**
- Consumes: `flip_timing_t` and timing-host functions from Task 1.
- Produces: a `Flip_GB` loop with no local timespec helpers, deadline state, FPS frame counter, direct `clock_nanosleep`, or late-frame schedule logic.

- [ ] **Step 1: Add `flip_timing.h` and Timing Host state**

Replace local timing state with:

```c
static flip_timing_t timing;
```

Keep `fps_overlay_enabled` and `fps_overlay_value` in `Flip_GB`, because they are presentation/UI state rather than pacing state.

- [ ] **Step 2: Remove local timing helpers**

Delete `timespec_add_ns()` and `timespec_after()` from `flip_gb.c`.

- [ ] **Step 3: Initialise the shared host**

Retain:

```c
double fps = av.timing.fps;
if (fps < 1.0)
    fps = 59.7275;
```

Then call:

```c
if (flip_timing_init(&timing, fps) < 0) {
    fprintf(stderr, "FLIPGB: timing initialization failed\n");
    /* existing orderly cleanup path */
}
```

Log `timing.frame_period_ns` instead of a local `frame_ns`.

- [ ] **Step 4: Replace local FPS accounting**

After handling logical system actions/hotkeys, call:

```c
double measured_fps = 0.0;
int fps_result = flip_timing_record_frame(&timing, &measured_fps);
```

When `fps_result > 0`, update `fps_overlay_value`, call `flip_video_set_fps_overlay()`, and retain the existing `FLIPGB_FPS` log.

- [ ] **Step 5: Preserve exit-before-sleep ordering**

Keep the `control_state.system.exit_requested` check after frame accounting and before `flip_timing_wait_frame()`.

- [ ] **Step 6: Replace local sleep/resync logic**

Call:

```c
int wait_result = flip_timing_wait_frame(&timing);
```

On `wait_result > 0`, retain the existing `FLIPGB: frame schedule resync` log. On `< 0`, log a timing wait failure and break.

- [ ] **Step 7: Verify no local timing implementation remains**

Run:

```bash
grep -nE 'timespec_add_ns|timespec_after|clock_nanosleep|fps_frames|fps_started|next_frame|frame_ns' src/emulators/flip_gb.c
```

Expected: no matches.

### Task 3: Build and hardware acceptance

**Files:**
- Modify after validation: `docs/ROADMAP.md`

**Interfaces:**
- Consumes: completed Timing Host integration.
- Produces: hardware-tested Phase 4A3 baseline suitable for `main`.

- [ ] **Step 1: Run native tests**

```bash
make test-controls
make test-timing
```

Both must pass.

- [ ] **Step 2: Run full clean ARM64 build**

```bash
make clean
make all
```

Expected: both `flip-frontend` and `flip-gb` link successfully; pre-existing Gambatte/libretro warnings are acceptable.

- [ ] **Step 3: Run source verification**

```bash
git diff --check
git status
grep -nE 'timespec_add_ns|timespec_after|clock_nanosleep|fps_frames|fps_started|next_frame|frame_ns' src/emulators/flip_gb.c || echo "PASS: Flip_GB timing implementation removed"
```

- [ ] **Step 4: Deploy only runtime binaries to FAT32 SD**

Copy only `build/flip-frontend` and `build/flip-gb` to `/mnt/h/flipos-dev/bin/`, sync, compare hashes, and unmount before removing the SD.

- [ ] **Step 5: Real-Miyoo-Flip acceptance**

Verify normal gameplay speed, stable FPS overlay near the target, MENU/GameSwitcher resume, MENU+R1 FPS toggle, volume/brightness shortcuts, START+SELECT exit, lid suspend/resume, controls after resume, and audio after resume.

- [ ] **Step 6: Preserve proven baseline**

Commit the exact tested source, push the feature branch, update all Phase 4A3 checklist items in `docs/ROADMAP.md`, and fast-forward `main` only after verification confirms the branch is based directly on the current known-good baseline.
