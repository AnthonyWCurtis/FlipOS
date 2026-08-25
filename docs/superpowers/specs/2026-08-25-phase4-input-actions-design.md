# FlipOS Phase 4A2B — Logical Controls and System Actions

## Status

Design approved for implementation on `phase4/input-actions`.

## Purpose

Phase 4A2A moved physical input ownership into the shared FlipOS runtime. Phase 4A2B separates that physical snapshot into two emulator-facing concepts:

1. normalized game controls for the emulated console;
2. FlipOS system actions for handheld/runtime policy.

The goal is that `flip-gb` no longer interprets Miyoo-specific button codes or physical shortcut chords directly.

## Architecture

```text
Miyoo hardware
    -> flip_input (physical snapshot)
    -> flip_controls
         -> flip_game_input_t
         -> flip_system_actions_t
    -> emulator host / shared runtime services
    -> thin core mapping
```

`flip_input` remains hardware acquisition only. It continues to own `/dev/input/event0`, `/dev/input/event1`, `/dev/ttyS1`, hall state, UART parsing and analogue centre calibration.

`flip_controls` is a pure runtime transformation layer. It has no device I/O and no emulator-core dependency.

## Normalized game controls

`flip_game_input_t` exposes platform-neutral game buttons:

- D-pad
- A/B/X/Y
- L1/R1/L2/R2
- Start/Select
- L3/R3
- left/right analogue axes

MENU and volume buttons are deliberately absent from game controls.

For Phase 4A2B analogue axes are centre-relative values (`raw - centre`) with no new deadzone, inversion, filtering or profile policy.

## System actions

`flip_system_actions_t` exposes one-update action pulses and current lid state:

- `gameswitcher_requested`
- `fps_toggle_requested`
- `exit_requested`
- `volume_step`
- `brightness_step`
- `lid_ready`
- `lid_closed`
- `lid_changed`
- `suspend_requested`

### Shortcut semantics

- MENU press arms a standalone action.
- MENU release with no consumed chord emits `gameswitcher_requested` once.
- MENU + R1 emits `fps_toggle_requested` once on the R1 press edge and consumes the pending standalone MENU action.
- VOL+ / VOL- emit one `volume_step` on the press edge.
- MENU + VOL+ / VOL- emit one `brightness_step` on the volume-key press edge and consume the pending standalone MENU action.
- START + SELECT emits `exit_requested` once when the chord becomes active. This is a global FlipOS system shortcut by design.
- The first authoritative lid sample reports `lid_changed` but does not request suspend.
- A later transition from lid open to lid closed emits `suspend_requested` once.

## Hotkey execution

`flip_hotkeys` remains the shared service that performs volume/brightness effects and OSD feedback. Phase 4A2B adds an action-based entry point so the emulator path can pass logical actions rather than a physical input snapshot.

The existing physical-input hotkey entry point remains temporarily for the frontend to avoid expanding this milestone into a frontend input refactor.

## Flip_GB changes

`flip-gb` keeps one physical snapshot per frame because it is still the current emulator host loop. Immediately after polling it transforms that snapshot through `flip_controls_update()`.

Gambatte input callbacks read only `flip_game_input_t`.

`flip-gb` reacts only to logical system actions for:

- lid-close save/suspend
- GameSwitcher request
- FPS overlay toggle
- START+SELECT exit
- volume/brightness handling

The save-state operation, process exit code and lifecycle implementation remain in `flip-gb` for now. Moving those responsibilities belongs to later Lifecycle / State milestones.

## Compatibility constraints

No intended change to:

- physical input device mapping
- analogue UART transport/calibration
- Game Boy control mapping
- MENU-alone GameSwitcher behavior
- MENU+R1 FPS toggle
- volume controls
- MENU+volume brightness controls
- lid suspend/resume behavior
- START+SELECT exit result
- state save/restore behavior
- audio/video/timing behavior

## Explicit non-goals

Phase 4A2B does not add:

- remapping
- controller profiles
- configurable deadzones
- analogue filtering
- multiple controllers
- SDL input
- `/dev/miyooio` integration
- Timing Host work
- Audio Host work
- Lifecycle Host work
- State Manager work

## Testing

A host-runnable pure unit test validates:

- game-button translation
- centre-relative analogue values
- MENU-alone release action
- MENU+R1 consumption and edge behavior
- START+SELECT edge behavior
- volume edges
- MENU+volume brightness and MENU consumption
- lid change and open-to-closed suspend request

Real Miyoo Flip acceptance testing must then verify the same hardware behavior that passed Phase 4A2A before this milestone can merge to `main`.
