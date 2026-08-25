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

- [ ] Move frame pacing out of Flip_GB
- [ ] Monotonic absolute frame schedule
- [ ] Sleep/resync policy
- [ ] FPS measurement
- [ ] Shared pacing policy

Timing Host must precede major Audio Host redesign because A/V sync issues have already been observed.

### Phase 4A4 — Global Audio Host

- [ ] ALSA/RK817 lifecycle ownership
- [ ] Shared audio buffering
- [ ] Sample handling
- [ ] Resampling strategy if required
- [ ] Latency strategy
- [ ] Suspend/resume recovery
- [ ] A/V synchronization policy

### Phase 4A5 — Lifecycle Host

- [ ] Start/suspend/resume/exit policy
- [ ] GameSwitcher request handling
- [ ] Lid transitions
- [ ] Cleanup ordering
- [ ] Timing resync after lifecycle interruption

### Phase 4B — Common Core Adapter API

- [ ] Define true core-facing FlipOS adapter interface
- [ ] Init/load/run/unload
- [ ] Video translation
- [ ] Audio translation
- [ ] Input mapping
- [ ] Serialize/unserialize
- [ ] Core/system information
- [ ] Clarify/rename existing frontend session executable adapter if needed

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
