# FlipOS Phase 4A2 Input Host Design

## Status

Approved design for Phase 4A2 on branch `phase4/input-host`.

Known-good baseline: `663d455 Establish FlipOS Phase 4A1 baseline`.

`main` remains the recovery/known-good branch until hardware validation completes.

## Goal

Extract the existing proven FlipOS input implementation from the hardware-test source tree into the runtime layer without changing observable input behaviour.

Phase 4A2 is an ownership and source-layout change, not an input feature redesign.

## Current State

The public API already lives in `include/flip_input.h` and is consumed by the frontend, global hotkeys, and the Game Boy emulator host.

The implementation currently lives in `src/hwtest/flip_input.c`, even though it is production runtime code and is built into both `flip-frontend` and `flip-gb`.

The Makefile currently builds `build/flip_input.o` from `src/hwtest/flip_input.c`.

## Design

### Public API

Keep `include/flip_input.h` compatible for Phase 4A2.

The existing `flip_input_state_t`, `flip_button_t`, `flip_input_open()`, `flip_input_close()`, `flip_input_poll()`, and `flip_button_name()` interfaces remain unchanged.

### Runtime ownership

Move the implementation to:

`src/runtime/flip_input.c`

The runtime Input Host continues to own:

- digital button input from `/dev/input/event0`
- confirmed Miyoo Flip button-to-FlipOS mappings
- hall wake/change events from `/dev/input/event1`
- authoritative persistent lid state from `/sys/devices/platform/hall-mh248/hallvalue`
- analogue stick transport from `/dev/ttyS1`
- confirmed UART packet format `FF YL XL YR XR FE`
- 60-frame centre calibration
- one shared `flip_input_state_t` snapshot consumed by frontend, hotkeys, and emulator host code

### Build integration

Change the Makefile rule for `build/flip_input.o` so it compiles `src/runtime/flip_input.c`.

Both `flip-frontend` and `flip-gb` continue linking the same shared object.

### Hardware-test ownership

Remove `src/hwtest/flip_input.c` after the runtime copy is established.

Hardware-test code should consume the same global Input Host API rather than maintaining a separate production input driver.

## Behaviour Invariants

Phase 4A2 must preserve the Phase 4A1 behaviour exactly:

- no button mapping changes
- no analogue axis reordering or inversion changes
- no calibration changes
- no polling cadence changes
- no hall/lid semantic changes
- no hotkey changes
- no libretro input callback behaviour changes
- no frontend navigation changes
- no GameSwitcher behaviour changes
- no suspend/resume behaviour changes

Gambatte continues taking one coherent host input snapshot per emulated frame; libretro `input_poll_cb()` remains passive and `input_state_cb()` reads the captured state.

## Explicit Non-Goals

Do not add or investigate the following in Phase 4A2:

- configurable button remapping
- configurable deadzones
- controller profiles
- multiple external controllers
- SDL input abstraction
- `/dev/miyooio` integration
- new analogue filtering
- input repeat redesign
- hotkey redesign
- power-button integration

These may be considered in later phases only after the global Input Host boundary is hardware-proven.

## Verification

### Source/build verification

Before device testing:

1. Confirm the runtime file is byte-for-byte equivalent in behaviour to the previous hardware-test implementation except for source ownership/comments if needed.
2. Confirm the Makefile points `flip_input.o` at `src/runtime/flip_input.c`.
3. Build `flip-frontend` and `flip-gb` with the existing AArch64 toolchain.
4. Review the complete diff against `main` and confirm no unrelated files or behaviour changes are present.

### Miyoo Flip hardware verification

Test from SD card only. Do not modify internal NAND/MTD.

Verify:

- D-pad: Up, Down, Left, Right
- face buttons: A, B, X, Y
- shoulders: L1, R1, L2, R2
- Start, Select, Menu
- L3 and R3
- Volume Up and Volume Down
- global volume hotkeys
- Menu+Volume brightness hotkeys
- left analogue stick
- right analogue stick
- frontend navigation
- Game Boy gameplay input
- no stuck or missing buttons after repeated presses
- lid close suspend path
- lid open wake/resume path
- input works normally after resume
- existing video and audio behaviour remains unchanged

## Merge Gate

Do not merge Phase 4A2 into `main` until:

- source/build verification passes
- the branch is tested on the actual Miyoo Flip
- all primary controls behave identically to the Phase 4A1 baseline
- suspend/resume input is confirmed working
- no regression is observed in frontend, Game Boy gameplay, global hotkeys, video, or audio

After hardware validation, Phase 4A2 can be committed/marked as the next known-good baseline and merged into `main`.
