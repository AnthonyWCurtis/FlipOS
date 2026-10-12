# FlipOS Architecture

This document describes the current FlipOS architecture as of the Beta 3
engineering audit. It is intentionally descriptive: it records how the system
is built today rather than proposing a rewrite.

## Scope and baseline

FlipOS is an SD-card-hosted userspace environment for the Miyoo Flip (RK3566).
The current release baseline is `0.1.0-beta.3`.

The default cross build targets AArch64 with `aarch64-linux-gnu-gcc/g++`.
Host-side tests are built with the native compiler. Third-party source is
vendored under `external/`; original FlipOS headers and sources live under
`include/`, `src/`, `tools/`, and `tests/`.

## Boot and shutdown lifecycle

The SD boot hook is `tools/device/updater-playtest`, installed as
`/mnt/SDCARD/.tmp_update/updater`.

At boot it performs these major steps:

1. Run the CPU Optimizer supervisor before attempting SD-card writes. A failed
   safety decision creates a RAM-only stock-safe latch.
2. Detect whether the SD filesystem is writable and select persistent or
   RAM-only logging accordingly.
3. Load the optional RK817 charging helper and start the charge service.
4. Start the FlipOS USB MTP/ADB presentation when its runtime is available.
5. Reconcile optimizer guard/calibration state and re-enforce stock-safe policy.
6. Handle pending optimizer validation, recovery and explicit optimizer
   sessions.
7. Optionally start playtest/performance capture tooling.
8. Launch `flip-frontend`.
9. When FlipOS exits, sync writable storage and return to the stock Miyoo
   launcher with `/usr/miyoo/bin/runmiyoo-original.sh`.

The optimizer boot path is deliberately fail-safe: missing or failed recovery
components must prefer stock-safe operation rather than a runtime voltage
profile.

## Frontend

`src/frontend/flip_frontend.c` is the main console UI. It owns navigation and
coordinates services rather than implementing their storage or hardware policy
directly.

Major frontend services include:

- library discovery/cache;
- recent and suspended-session presentation;
- settings, time, Wi-Fi, BIOS and update screens;
- CPU Optimizer UI and validation launch;
- global volume/brightness actions;
- lid-triggered frontend sleep/wake;
- battery status and low-battery shutdown policy.

Rendering uses FlipOS' direct DRM drawing helpers rather than SDL or a desktop
windowing stack.

## Emulator path

The public emulator executables are `flip-gb` and `flip-gba`.

Both are intentionally thin entry points:

`flip-gb/flip-gba -> flip_emulator_runner -> flip_core_host -> core adapter`

Core adapters currently wrap:

- Gambatte for Game Boy / Game Boy Color;
- mGBA for Game Boy Advance.

The shared Core Host coordinates:

- physical input polling;
- logical controls and system actions;
- cartridge save and RTC persistence;
- save-state restore/save;
- Save Safety recovery;
- frame timing;
- audio buffering/resampling/output;
- video presentation;
- telemetry/performance diagnostics;
- low-battery protection;
- dynamic CPU ceiling adjustment;
- lifecycle requests such as GameSwitcher and lid suspension.

This shared path is important: emulator-specific entry points should not grow
copies of lifecycle, persistence or hardware policy.

## Sessions and GameSwitcher

Session state is split into small focused modules:

- `flip_session.c` — process/session orchestration;
- `flip_session_registry.c` — suspended-session registry;
- `flip_session_persistence.c` — current session record;
- `flip_session_adapter.c` — system-specific process/exit mapping;
- `flip_system.c` — supported-system registry;
- `flip_exit_protocol.c` — shared exit codes and meanings.

The frontend reconciles the session registry during startup and uses it to
present suspended games. Emulator lifecycle exits are mapped back into session
and GameSwitcher behavior through these shared interfaces.

## Saves, states and recovery

User data protection is divided into:

- `flip_save.c` — cartridge save/RTC generations;
- `flip_state.c` — emulator state generations;
- `flip_recovery.c` — paired Save Safety timeline recovery;
- `flip_emulator_runner.c` — integration only, not persistence internals.

Current and previous generations are retained, with temporary files and rename
promotion used for normal writes. Recovery markers prevent an incompatible
savestate from being resumed after a cartridge-save timeline change.

Save/state/recovery code is data-integrity critical and must be changed in small
transactional steps with explicit failure tests.

## Input and lifecycle

`flip_input.c` reads physical controls, analogue UART data and lid state.

`flip_controls.c` converts physical input into:

- game input delivered to the emulator core; and
- FlipOS system actions such as volume, brightness, GameSwitcher, exit and lid
  suspension.

`flip_hotkeys.c` applies the volume/brightness policy represented by those
actions.

`flip_lifecycle.c` owns common emulator exit/save behavior so individual
cores do not implement separate suspend/GameSwitcher rules.

## Audio and video

Audio is handled by `src/audio/` and the shared Core Host. Core-rate audio is
resampled to the hardware output rate and queued to a writer thread feeding the
stock ALSA `aplay` path. The PCM pipe is non-blocking: transport writes have a
bounded deadline, and `aplay` shutdown escalates through bounded clean-exit,
SIGTERM and SIGKILL waits so a wedged ALSA child cannot indefinitely hold game
exit. RK817 routing and the confirmed deep-resume codec repair are kept in the
audio hardware layer.

Video is handled by `flip_video.c`, layout code and the GPU/DRM helpers.
Low-level DRM drawing is also used by the frontend and optimizer progress
screens.

The GPU service/sender targets exist as explicit build targets but are not part
of the default `all` target or current release payload.

## Power and performance

Power-related responsibilities are intentionally split:

- battery sampling and low-battery policy;
- backlight/power/lid handling;
- charger policy and RK817 helper;
- CPU policy/profile/dynamic ceiling code;
- telemetry and performance diagnostics;
- CPU Optimizer calibration, guard, supervisor and validation tools.

Voltage/OPP experimentation is runtime-only and guarded by recovery mechanisms.
Persistent boot-time voltage/device-tree changes are outside the safe audit
scope unless separately approved and proven recoverable.

## CPU Optimizer safety architecture

The optimizer is intentionally separated into multiple small components:

- supervisor — earliest boot safety decision;
- guard — durable trial/recovery state;
- calibration — per-device search state;
- session controller — explicit optimizer workflow;
- candidate runner — bounded live stress/validation;
- deadman and hardware-recovery helpers;
- progress/validation UI;
- runtime-profile loader.

Calibration and final validation use different stress policies. Hardware and
software recovery remain safety-critical and must not be simplified merely to
reduce file count.

## Updates

The update path is:

`frontend -> flip-update-fetch -> staged FlipOSUpdate package -> flip-update-install`

The fetcher selects a GitHub Release asset, downloads it, validates the package
manifest/checksums and stages it outside the live runtime.

The installer validates an allowlisted payload, stages and re-hashes it, then
creates a complete rollback snapshot before mutating the live runtime. Update
installation is a versioned transaction:

- a durable lock identifies an incomplete transaction;
- `prepared` is synced only after rollback data is complete;
- each live path is appended to a write-ahead rollback journal and synced
  before its atomic rename;
- the separately installed SD boot hook has its own write-ahead marker;
- `committed` is the durable decision point after runtime, boot hook and
  `last-good` metadata are synced.

On boot, a surviving transaction lock is handled before any FlipOS runtime
component is executed. Pre-commit transactions restore the previous runtime;
post-commit transactions keep the new runtime and finalize the lock. Unknown or
failed recovery falls back to the stock Miyoo launcher rather than executing a
potentially mixed FlipOS runtime.

Update handling must preserve user state; release packages intentionally contain
runtime binaries/configuration/modules and not user saves/settings. The
transaction/recovery protocol is compatibility-sensitive: future installer
changes must continue to understand existing transaction formats or explicitly
fail closed.

## Build, tests and release tooling

The top-level `Makefile` contains cross-build targets, host unit tests,
architecture tests and a `verify` aggregate.

Important output groups include:

- frontend;
- GB/GBA emulator executables;
- MTP responder;
- charge service;
- optimizer/recovery tools;
- Wi-Fi/performance diagnostic tools;
- four hardware helper kernel modules.

Release builders live under `tools/update/`.

The test suite combines C unit tests with Python architecture/packaging tests.
Real hardware acceptance remains mandatory for device lifecycle, hardware
register, suspend/resume, audio, display, charging and voltage behavior.

## Third-party code

Vendored code under `external/` is not treated as FlipOS style-cleanup
material:

- Gambatte/libretro — GPLv2;
- mGBA — MPL 2.0;
- uMTP-Responder — GPLv3 or later.

FlipOS-specific provenance and license notices are retained in
`docs/THIRD_PARTY_NOTICES.md`.

## Engineering rule

Prefer fixing concrete defects inside the existing architecture over replacing
working subsystems. Shared policy belongs in shared modules; hardware-critical
recovery paths should remain small, explicit and independently testable.
