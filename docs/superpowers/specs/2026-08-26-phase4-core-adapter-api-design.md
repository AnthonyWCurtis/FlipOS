# Phase 4B Common Core Adapter API Design

## Status

Approved architectural direction for Phase 4B.

## Goal

Refactor the existing Gambatte-based Game Boy runtime so FlipOS owns a reusable core-host execution path and Gambatte is accessed through a small core-neutral adapter interface.

Phase 4B does not add any new emulator core or playable system. Game Boy through Gambatte remains the only emulator path implemented and hardware-tested during this phase.

## Project constraints

- SD-card-first development only.
- Do not modify internal NAND/MTD.
- Preserve the stock Miyoo recovery path.
- Keep `main` known-good and real-hardware tested.
- Make changes on `phase4/core-adapter-api` until hardware validation passes.
- Do not add mGBA, NES, SNES, or any other new emulator/core in Phase 4B.
- Do not implement the Phase 5 State Manager in Phase 4B.
- Do not redesign the existing frontend/session exit-code protocol in Phase 4B.
- Do not change the validated Video, Input, Timing, Audio, or Lifecycle Host hardware behaviour unless required to expose a neutral interface.

## Problem statement

After Phases 4A1 through 4A5, FlipOS has shared hosts for video, input, controls, timing, audio, and lifecycle policy. However, `src/emulators/flip_gb.c` still combines two different responsibilities:

1. Gambatte/libretro-specific integration, callbacks, ROM loading, serialization calls, and input-ID translation.
2. The reusable FlipOS emulator execution loop that polls input, updates controls, runs one emulated frame, flushes audio, handles lifecycle decisions, updates hotkeys/FPS, and performs timing waits.

If a second core were added later while this structure remained, the easiest path would be to copy that host loop. Phase 4B removes that duplication risk before any second core is introduced.

There is also a naming collision in the current architecture. `flip_emulator_adapter_t` is a frontend/session-process adapter used to map child exit codes and test resume availability; it is not an emulator-core adapter. Phase 4B renames this concept so the new core adapter can use unambiguous terminology.

## Chosen architecture

### High-level structure

```text
flip-gb executable
        |
        | selects/configures Gambatte adapter
        v
FlipOS Core Host
        |
        +-- Video Host
        +-- Audio Host
        +-- Input / Controls Host
        +-- Timing Host
        +-- Lifecycle Host
        +-- Hotkeys / FPS diagnostics
        |
        v
flip_core_adapter_t
        |
        v
Gambatte / libretro
```

The Core Host owns the reusable handheld execution loop. The adapter owns only the translation between FlipOS core-neutral concepts and Gambatte/libretro.

### Why this approach

This phase deliberately creates both the neutral adapter API and the common host loop. Defining only an interface while leaving the execution loop inside `flip_gb.c` would not remove the main source of future duplication. Conversely, making the host itself libretro-specific would unnecessarily couple FlipOS to one emulator API.

The neutral boundary is therefore FlipOS-defined, with Gambatte/libretro behind the adapter.

## Core adapter API

Create a new public API in `include/flip_core.h`.

The exact implementation plan may refine names, but the interface must provide these responsibilities:

```text
adapter initialization
adapter deinitialization
load game
unload game
read core/system information
accept normalized FlipOS game input
run exactly one emulated frame
serialize size
serialize current core state
unserialize core state
```

The interface must not expose libretro types to the shared Core Host.

### Core information

The adapter reports a small neutral information structure containing at least:

- core name
- core version when available
- target frame rate
- source audio sample rate
- nominal/base video dimensions when available

The host may continue to apply the existing Game Boy fallback FPS when a core reports an invalid rate, but that fallback remains system/Gambatte configuration rather than a universal assumption.

### Input

The shared host polls hardware and obtains `flip_controls_state_t` exactly as it does now.

Only `flip_controls_state_t.game` is passed to the core adapter.

The Gambatte adapter translates normalized FlipOS buttons such as `FLIP_GAME_A`, `FLIP_GAME_B`, directions, START, and SELECT to the corresponding libretro joypad state. Miyoo-specific physical button knowledge must not move into the adapter.

### Video output

Gambatte/libretro video callbacks remain adapter-owned. The adapter converts the active libretro pixel format into the existing `flip_video_format_t` and submits raw frames to `flip_video_submit()`.

Scaling, DRM ownership, OSD composition, centering, and presentation remain in the Video Host.

### Audio output

Gambatte/libretro audio callbacks remain adapter-owned. They submit interleaved stereo PCM through `flip_audio_submit()`.

The shared Core Host calls `flip_audio_end_frame()` once after each successful core frame so no FlipOS-owned PCM staging remains across the emulated frame boundary.

Physical PCM configuration, RK817 setup/recovery, volume, and transport remain in the Audio Host.

## Shared Core Host

Create a reusable Core Host API in `include/flip_core_host.h` with implementation in `src/runtime/flip_core_host.c`.

The host owns the established frame order:

```text
poll physical input
-> update controls
-> pass normalized game input to adapter
-> run exactly one core frame
-> flush Audio Host frame
-> Lifecycle Host immediate checkpoint
-> FPS toggle / global hotkey actions
-> Timing Host FPS accounting
-> Lifecycle Host after-frame checkpoint
-> Timing Host wait/resync policy
```

This ordering is a compatibility requirement because it preserves the hardware-tested semantics established by 4A2B through 4A5.

### Lifecycle interaction

The Core Host calls the existing Lifecycle Host at the same two checkpoints used by the hardware-tested Gambatte path:

- immediate checkpoint after the emulated frame/audio flush for suspend and GameSwitcher requests
- after-frame checkpoint after FPS/hotkey accounting for normal global exit

The Core Host receives a save callback from the runner/persistence layer and passes it to the Lifecycle Host. Lifecycle policy remains shared; persistence implementation remains outside the Core Host.

### Resume timing

When a saved game state was successfully restored before entering the frame loop, the Core Host invokes `flip_lifecycle_resume_timing()` so the Timing Host starts from a fresh wall-clock schedule instead of attempting to catch up.

## Gambatte adapter

Create a focused Gambatte implementation, expected as:

```text
include/flip_gambatte_core.h
src/emulators/flip_gambatte_core.c
```

It owns only Gambatte/libretro integration:

- libretro environment callback
- video callback and pixel-format translation
- audio callbacks
- passive libretro input-poll callback
- normalized FlipOS input to libretro joypad translation
- `retro_init` / `retro_deinit`
- ROM load into Gambatte
- `retro_run`
- `retro_serialize_size`
- `retro_serialize`
- `retro_unserialize`
- `retro_unload_game`
- core/system information translation

It must not poll Miyoo hardware, own DRM, perform frame pacing, interpret FlipOS system actions, perform RK817 recovery, or decide GameSwitcher/exit policy.

## `flip-gb` executable after 4B

`src/emulators/flip_gb.c` remains the Game Boy executable but becomes a composition/root layer rather than the emulator host implementation.

Its responsibilities are limited to:

- validate the ROM argument
- configure/select the Gambatte adapter
- provide current temporary Game Boy resume-state persistence
- invoke the shared Core Host
- return the lifecycle-selected process exit code

The existing Game Boy state-file format and paths remain unchanged in Phase 4B.

## Save/state boundary

The core adapter exposes only raw serialization capabilities:

```text
serialize_size
serialize
unserialize
```

Phase 4B does not introduce shared state paths, hashes, backups, atomic state-manager APIs, recovery UI, or save-corruption handling.

The existing GB automatic-resume persistence remains temporary runner-owned code and continues to use the current path/hash/state format. The Core Host receives callbacks for save/restore availability rather than becoming aware of GB file paths.

Phase 5 will replace this temporary persistence layer with the shared State Manager while continuing to use the same core serialization interface.

## Session adapter rename

Rename the existing frontend-side `flip_emulator_adapter_t` concept to `flip_session_adapter_t` because it maps emulator-process results, not emulator-core operations.

Expected rename scope:

```text
include/flip_emulator_adapter.h
    -> include/flip_session_adapter.h

src/runtime/flip_emulator_adapter.c
    -> src/runtime/flip_session_adapter.c

flip_emulator_adapter_t
    -> flip_session_adapter_t

flip_emulator_adapter_for_system()
    -> flip_session_adapter_for_system()
```

`flip_system_t.adapter` should also be renamed to make its session/process role explicit, for example `session_adapter`.

This is a naming/clarity change only. Existing exit-code mapping and resume-availability behaviour must remain unchanged.

## Error handling

The shared host treats core initialization/load failures as startup failures and returns without entering the frame loop.

During the frame loop:

- input/control failures terminate the emulator cleanly
- adapter frame-run failure terminates cleanly
- audio frame-flush failure is logged and may continue silent according to the existing Audio Host behaviour
- lifecycle save failure preserves the existing Lifecycle Host policy
- timing/FPS failures terminate cleanly
- cleanup must continue through adapter unload/deinit and the existing shared lifecycle shutdown path

No new crash-recovery protocol is introduced in this phase.

## Testing strategy

### Native tests

Add a fake core adapter used only by host tests. It is not a playable emulator core.

The fake adapter verifies:

- init/load/info/run/unload/deinit ordering
- one normalized input snapshot is supplied before each frame
- exactly one core frame is run per host iteration
- serialize/unserialize calls are routed through the neutral API
- invalid/missing required adapter operations are rejected
- lifecycle exit codes are propagated correctly
- resume timing resync is requested only when a state was actually restored

Tests must compile on the WSL host with `-std=c11 -O2 -Wall -Wextra -Werror` where practical.

### Existing regression gates

Run all existing host tests:

- controls
- timing
- audio
- lifecycle

Then run the new core/core-host tests and a full ARM64 static build.

### Real Miyoo Flip validation

Only Gambatte/GB is hardware-tested in Phase 4B.

Acceptance covers:

- boot and frontend unchanged
- launch existing GB ROM
- video/audio/input unchanged
- FPS approximately the current ~59.7 target
- volume/brightness/FPS hotkeys unchanged
- GameSwitcher save/resume unchanged
- switch between existing GB titles and resume correctly
- lid suspend/resume unchanged
- RK817 deep-resume audio recovery unchanged
- START+SELECT normal exit unchanged
- automatic resume-state compatibility with states created before 4B
- runtime logs show the common Core Host path without new errors

No second emulator is required or permitted for 4B acceptance.

## Planned file responsibilities

```text
include/flip_core.h
    core-neutral adapter contracts and data structures

include/flip_core_host.h
src/runtime/flip_core_host.c
    reusable emulator execution-loop ownership

include/flip_gambatte_core.h
src/emulators/flip_gambatte_core.c
    Gambatte/libretro translation only

src/emulators/flip_gb.c
    GB executable composition + temporary GB state persistence

include/flip_session_adapter.h
src/runtime/flip_session_adapter.c
    renamed frontend/session process protocol adapter

tests/test_flip_core.c
    adapter contract tests

tests/test_flip_core_host.c
    fake-adapter host orchestration/order tests
```

The implementation may combine `test_flip_core.c` and `test_flip_core_host.c` if doing so keeps the tests clearer, but production responsibilities must remain separated.

## Explicitly excluded

Phase 4B does not include:

- any second emulator core
- mGBA or GBA support
- NES/SNES/other systems
- runtime-selectable core plugins
- dynamic loading of arbitrary cores
- Phase 5 State Manager
- save backup/recovery architecture
- GameSwitcher/session-manager redesign
- exit-code protocol redesign
- direct ALSA rewrite
- audio resampler implementation unless existing Gambatte behaviour unexpectedly requires it
- performance/frame-pacing tuning
- NAND/MTD modification

## Completion criteria

Phase 4B is complete only when:

1. A core-neutral adapter API exists without libretro types leaking into the shared Core Host.
2. The reusable frame/lifecycle/timing loop is owned by the Core Host rather than `flip_gb.c`.
3. Gambatte runs through the new adapter.
4. `flip_gb.c` is reduced to GB composition and temporary persistence responsibilities.
5. The existing process/session adapter has unambiguous session-oriented naming.
6. Native core/host tests pass.
7. All existing regression tests pass.
8. The full ARM64 build passes.
9. Existing Game Boy behaviour and existing resume states pass real-Miyoo-Flip hardware validation.
10. No new emulator core has been added.
