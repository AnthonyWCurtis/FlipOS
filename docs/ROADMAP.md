# FlipOS Canonical Roadmap

This file is the canonical implementation checklist for FlipOS. Milestones are checked off only after the required build and real-Miyoo-Flip hardware validation pass.

## Core project rules

- SD-card-first development.
- Do not modify internal NAND/MTD unless explicitly discussed, justified and approved.
- Preserve the stock Miyoo recovery path.
- Base hardware claims on actual Miyoo Flip evidence whenever possible.
- Keep `main` known-good and hardware-tested.
- Do architectural work on feature branches.
- Prefer small, bootable, testable milestones.
- Core gaming UX takes priority over optional features.

## Phase 0 — Hardware Understanding

- [x] SD boot hook demonstrated
- [x] Input/display/audio/power probes completed sufficiently for runtime development
- [x] FAT32/vfat SD behavior verified
- [ ] Deeper `/dev/miyooio` investigation
- [ ] Continue hardware documentation as new behavior is verified

## Phase 1 — Native Runtime

- [x] Native FlipOS launches from SD
- [x] Direct DRM display path
- [x] Direct input path
- [x] RK817/ALSA audio path
- [x] Brightness control
- [x] Frontend and emulator process model

## Phase 2 — First Game

- [x] Gambatte native Game Boy proof of concept
- [x] ROM launch
- [x] Video
- [x] Audio
- [x] Input
- [x] Save states
- [x] Automatic resume
- [x] Clean exit paths

## Phase 3 — Frontend / Library

- [x] Games -> Systems -> Games browser
- [x] Show only systems containing ROMs
- [x] Generic system registry
- [x] GB registry entry
- [x] Smart library cache
- [x] FAT32-safe fingerprinting
- [x] Cache HIT path
- [x] Corrupt index recovery
- [x] Atomic index/fingerprint commit ordering
- [x] Library update OSD

## Phase 4 — Emulator Host Refactor

### Phase 4A1 — Global Video Host

- [x] Shared Video Host implemented
- [x] Flip_GB submits raw frames to host
- [x] Real-hardware validation passed

Known-good milestone: `663d455` initially; incorporated into later known-good `main` baselines.

### Phase 4A2 — Global Input Host

#### Phase 4A2A — Physical input ownership

- [x] Physical Input Host moved into runtime layer
- [x] One coherent input snapshot per emulated frame
- [x] Real-hardware validation passed

Known-good baseline after 4A2A: `04653d7`.

#### Phase 4A2B — Game controls vs system actions

- [x] Pure logical-controls transformation layer
- [x] Normalized game-control state
- [x] Shared logical system actions
- [x] START+SELECT global exit action
- [x] Flip_GB no longer interprets Miyoo-specific system buttons directly
- [x] Host unit tests pass
- [x] Full WSL build passes
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `8938852` (`Separate game controls from system actions`).

### Phase 4A3 — Global Timing Host

- [x] Move frame pacing out of Flip_GB
- [x] Monotonic absolute frame schedule
- [x] Sleep/resync policy
- [x] FPS measurement
- [x] Shared pacing policy
- [x] Host unit tests pass
- [x] Full WSL build passes
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `ae8ac10` (`Move frame pacing into timing host`).

Timing Host precedes major Audio Host redesign because A/V sync issues have already been observed.

### Phase 4A4 — Global Audio Host

- [x] ALSA/RK817 lifecycle ownership
- [x] Shared audio buffering
- [x] Sample handling
- [x] Resampling strategy if required
- [x] Latency strategy
- [x] Suspend/resume recovery
- [x] A/V synchronization policy
- [x] Host unit tests pass
- [x] Full WSL build passes
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `5df7461` (`Route Gambatte audio through audio host`).

Current strategy: emulator cores retain their native source rate; the shared Audio Host converts to the verified Miyoo Flip physical playback rate. Gambatte remains 32768 Hz stereo S16_LE and is streamed through the persistent linear resampler to `aplay`/RK817 at exactly 32000 Hz. Hardware verification showed the previous 32768-to-32000 clock mismatch caused progressive synchronous-write backpressure; after conversion, long-run transport writes remained sub-millisecond and frame pacing returned to target. Deep-resume RK817 recovery remains limited to the exact confirmed broken `17=06 38=00 40=69` signature. Hardware logging has also observed `17=40 38=00 40=a5` during working playback, so that state remains an alternate observed working state and must not trigger speculative recovery writes.

### Phase 4A5 — Lifecycle Host

- [x] Start/suspend/resume/exit policy
- [x] GameSwitcher request handling
- [x] Lid transitions
- [x] Cleanup ordering
- [x] Timing resync after lifecycle interruption
- [x] Host unit tests pass
- [x] Full WSL build passes
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `bbe6456` (`Route emulator lifecycle through lifecycle host`).

Lifecycle policy now lives in the shared host while core serialization remains in the GB adapter until the Phase 5 State Manager work. Suspend/GameSwitcher require a successful save before returning exit code 20; normal START+SELECT exit still attempts a save but exits even if that save fails. Shared cleanup order is settings flush, audio close, input close, then video close, and restored sessions explicitly resync the Timing Host before entering the frame loop.

### Phase 4B — Common Core Adapter API

- [x] Define true core-facing FlipOS adapter interface
- [x] Init/load/run/unload
- [x] Video translation
- [x] Audio translation
- [x] Input mapping
- [x] Serialize/unserialize
- [x] Core/system information
- [x] Clarify/rename existing frontend session executable adapter if needed
- [x] Shared Core Host owns reusable frame orchestration
- [x] Gambatte isolated behind a thin core adapter
- [x] Flip_GB reduced to composition and temporary GB persistence
- [x] Full native regression suite and ARM64 static build pass
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `98db8c3` (`Merge branch 'phase4/audio-rate-conversion' into phase4/core-adapter-api`). The tested `flip-gb` SHA-256 was `ce34d47740f07583c008456b3ef271a6239030e97fa975944c3c776cc5cd13d2`. Hardware acceptance included normal GB launch/runtime paths, lifecycle/resume behavior, exact RK817 deep-resume recovery, verified physical `rate: 32000`, stable long-run audio transport with no progressive backlog, and frame pacing at the expected ~59.7 FPS target.

## Phase 5 — Save / State Architecture

- [ ] Shared State Manager
- [ ] State path generation
- [ ] ROM identity/hash
- [ ] Atomic state write
- [ ] fsync/rename safety
- [ ] Restore validation
- [ ] Shared serialization storage
- [ ] Session-owned save/restore policy
- [ ] Save Safety / Recovery design
- [ ] Rotating save backups
- [ ] Previous-known-good save
- [ ] Recovery UI/tool

## Phase 6 — Global GameSwitcher / Session Manager

- [ ] Suspend current game
- [ ] Save state
- [ ] Record active session
- [ ] Switch games
- [ ] Resume previous game
- [ ] Close/remove sessions
- [ ] Prune stale sessions
- [ ] Recover interrupted sessions
- [ ] Clean up exit-code protocol

## Phase 7 — System Services

- [ ] Formal Settings service
- [ ] Formal OSD service
- [ ] Formal Power/lid service
- [ ] Save Manager
- [ ] State Manager
- [ ] Library/cache service
- [ ] BIOS manager
- [ ] Logging
- [ ] Battery service
- [ ] Performance profiles
- [ ] Paths/storage service
- [ ] Session persistence

## Phase 8 — Second Emulator / Architecture Proof

Preferred system: Game Boy Advance with mGBA.

- [ ] Flip_GBA executable
- [ ] Reuse same Video Host
- [ ] Reuse same Audio Host
- [ ] Reuse same Input Host
- [ ] Reuse same Timing Host
- [ ] Reuse same Lifecycle Host
- [ ] Reuse same State/Save Managers
- [ ] Reuse same GameSwitcher
- [ ] Thin mGBA adapter only

Architecture rule: if adding GBA requires copying Miyoo-specific video/audio/input/lifecycle code from Flip_GB, fix the abstraction before continuing.

## Phase 9 — Emulator Ecosystem

- [ ] NES
- [ ] SNES
- [ ] Mega Drive / Genesis
- [ ] Game Gear
- [ ] Master System
- [ ] PC Engine
- [ ] Additional RK3566-appropriate systems

Each system should primarily require a core, thin adapter, metadata/configuration and BIOS rules where applicable.

## Phase 10 — Performance / Power

- [ ] CPU governor policy
- [ ] Per-system performance profiles
- [ ] Suspend optimization
- [ ] Battery drain investigation
- [ ] Thermal behavior
- [ ] Frame pacing optimization
- [ ] Audio latency tuning
- [ ] Idle behavior
- [ ] Undervolting only if hardware support and safety are verified

## Phase 11 — UX Refinement

- [ ] Fast boot-to-library
- [ ] Instant-feeling resume
- [ ] Polished GameSwitcher
- [ ] Consistent global hotkeys
- [ ] Minimal settings friction
- [ ] Clear OSD
- [ ] Favorites/recent games
- [ ] Optional history
- [ ] Polished shutdown/suspend
- [ ] Save corruption protection UX
- [ ] State/save recovery tools
- [ ] Crash/session recovery
- [ ] Battery-aware suspend behavior
- [ ] Optional performance profiles
- [ ] Library validation/rebuild tools

## Phase 12 — Distribution / Update / Recovery

- [ ] Clean runtime-only SD installation
- [ ] Versioning
- [ ] Update mechanism
- [ ] Rollback support
- [ ] Recovery documentation
- [ ] Diagnostics export

Internal NAND/MTD replacement remains outside the normal FlipOS roadmap and would require a separate future decision after the SD-based system is mature and proven.
