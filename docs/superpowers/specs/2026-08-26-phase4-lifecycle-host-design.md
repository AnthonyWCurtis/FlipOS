# Phase 4A5 — Lifecycle Host Design

## Status

Approved for implementation on `phase4/lifecycle-host` from hardware-tested `main` commit `41d3b53`.

## Goal

Move handheld lifecycle policy out of `flip-gb` into a shared FlipOS Lifecycle Host without pulling Phase 5 Save/State architecture or Phase 4B Core Adapter work forward.

The host owns when an emulator session should save and leave, which exit protocol is used, shared-host cleanup ordering, and timing resynchronization after a resumed session. Core-specific serialization and core unload/deinit remain in `flip-gb` until their later roadmap phases.

## Non-goals

- No change to the save-state file format, path, hashing, atomic write, or restore implementation.
- No new GameSwitcher/session-manager protocol.
- No change to frontend exit-code `20` behavior.
- No direct deep-suspend call from the emulator process.
- No common libretro/Core Adapter API yet.
- No performance, audio-latency, or frame-pacing tuning.
- No NAND/MTD changes.

## Existing behavior to preserve

`flip-gb` currently receives logical system actions from `flip_controls` after `retro_run()`.

The important ordering is:

1. poll physical input
2. update logical controls/actions
3. `retro_run()`
4. flush Audio Host frame data
5. lid/suspend handling
6. FPS toggle
7. GameSwitcher handling
8. volume/brightness hotkeys
9. Timing Host FPS accounting
10. START+SELECT exit
11. Timing Host wait/pacing

Suspend and GameSwitcher requests save state and leave with exit code `20` only when the save succeeds. START+SELECT attempts a save but exits normally even if that save fails.

That ordering must remain observable after 4A5.

## Architecture

```text
flip_controls
    ↓ logical system actions
flip_lifecycle
    ├── lid transition reporting
    ├── immediate checkpoint
    │     ├── suspend request
    │     └── GameSwitcher request
    ├── after-frame checkpoint
    │     └── normal exit request
    ├── save-before-leave policy
    ├── exit-code policy
    ├── shared cleanup ordering
    └── Timing Host resync on resumed session
           ↓
flip-gb provides only core-specific save callback
```

The lifecycle host uses checkpoints deliberately. A single early lifecycle call would move START+SELECT before FPS accounting and change proven behavior. Two explicit checkpoints keep policy shared while retaining the current frame semantics.

## Public API

Create `include/flip_lifecycle.h` and `src/runtime/flip_lifecycle.c`.

```c
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
```

## Checkpoint policy

### Immediate checkpoint

Runs immediately after `retro_run()` and `flip_audio_end_frame()`.

Priority is the same as the existing code:

1. suspend request
2. GameSwitcher request

For suspend or GameSwitcher:

- invoke the supplied save callback;
- on save success, request process exit with code `20`;
- on save failure, do not exit;
- if suspend save fails and GameSwitcher is also present, continue to the GameSwitcher attempt, matching the old sequential blocks.

START+SELECT is not handled at this checkpoint.

### After-frame checkpoint

Runs after hotkeys and Timing Host FPS accounting, but before Timing Host wait.

Only normal exit is handled here.

- invoke the supplied save callback;
- exit code is `0` whether the save succeeds or fails;
- report the save result for diagnostics.

This preserves the proven START+SELECT exit-before-sleep behavior.

## Lid transitions

`flip_lifecycle_handle()` copies `lid_changed` and `lid_closed` into the result so lifecycle diagnostics are no longer tied to `flip-gb`.

The actual hall sensor acquisition remains in Input Host and logical transition generation remains in Controls Host.

## Save callback boundary

4A5 does not know how Gambatte serializes state.

`flip-gb` passes a small callback that calls its existing `save_resume_state()` implementation. This makes save-before-leave policy reusable while leaving state storage mechanics for Phase 5.

## Shared cleanup ordering

`flip_lifecycle_shutdown_shared()` owns the shutdown order for shared FlipOS hosts:

1. `flip_settings_flush()`
2. `flip_audio_close()`
3. `flip_input_close()`
4. `flip_video_close()`

Core-specific cleanup still happens first in `flip-gb`:

1. `retro_unload_game()` / ROM memory release
2. `retro_deinit()`
3. `flip_lifecycle_shutdown_shared()`

This preserves current core teardown ordering while removing shared-host cleanup knowledge from the emulator adapter.

A settings flush failure is logged and returned as `-1`, but cleanup continues through audio/input/video.

## Timing resync

`flip_lifecycle_resume_timing()` delegates to `flip_timing_resync()`.

When `load_resume_state()` reports a real restored session, `flip-gb` calls this after Timing Host initialization and before entering the frame loop. New sessions do not perform an unnecessary lifecycle resync.

This establishes the lifecycle-owned timing-resync boundary required by later in-process lifecycle work without redesigning the current process-based GameSwitcher.

## Diagnostics

Lifecycle Host logs shared lifecycle policy using `FLIP_LIFECYCLE:` prefixes. Core serialization errors may continue to use the existing `FLIPGB_STATE:` diagnostics.

Hardware logs should make it possible to distinguish:

- lid transition;
- suspend/GameSwitcher/exit request;
- save success/failure policy result;
- timing resync on restored session;
- settings flush failure during cleanup.

## Tests

Native host tests must cover:

- no lifecycle action;
- lid transition propagation;
- suspend save success → exit `20`;
- suspend save failure → no exit;
- failed suspend followed by successful simultaneous GameSwitcher → exit `20`;
- GameSwitcher save success → exit `20`;
- immediate checkpoint ignores normal exit;
- after-frame checkpoint handles normal exit;
- normal exit occurs even when save fails;
- after-frame checkpoint ignores suspend/GameSwitcher;
- lifecycle timing resume resets Timing Host FPS counters;
- shared cleanup invokes settings/audio/input/video in the required order and continues after a settings flush failure.

## Hardware acceptance

On the real Miyoo Flip:

- normal GB gameplay unchanged;
- MENU alone opens GameSwitcher and resume remains correct;
- lid close saves/leaves and frontend deep-suspends as before;
- lid reopen resumes game and audio/input return;
- START+SELECT saves and exits normally;
- volume/brightness hotkeys remain functional;
- FPS remains near the existing target and no catch-up burst appears after resume;
- logs show lifecycle reasons, timing resync on restored sessions, and no new cleanup/audio failures.

`main` is updated only after full WSL build and real-hardware acceptance pass.
