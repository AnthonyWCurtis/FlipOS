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

### Phase 5A — Shared State Manager

- [x] Shared State Manager
- [x] State path generation
- [x] ROM identity/hash
- [x] Atomic state write
- [x] File-data fsync + rename safety
- [x] Restore validation
- [x] Shared serialization storage
- [x] Preserve legacy GB `.state` identity and raw core payload format
- [x] Previous-known-good state semantics
- [x] Current-state corruption fallback to `.state.prev`
- [x] Failed save leaves committed state recoverable
- [x] GB persistence removed from `flip_gb.c` and routed through State Manager
- [x] Existing Lifecycle Host save/restore policy and exit-code-20 behavior preserved
- [x] Full native regression suite and ARM64 static build pass
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `cf05ce6` (`Update GB architecture test for State Manager`). The exact tested `flip-gb` SHA-256 was `a5ae2985c56a3cf0b6558afdfe3dde8a86885f1e248bde4fc3a33d0b7ab54feb`.

Hardware acceptance verified direct restore of a pre-Phase-5 legacy GB state, creation of a new current state while preserving the successfully restored legacy state as `.state.prev`, controlled truncation of the current state with successful fallback to `.state.prev`, normal START+SELECT save/exit, lid suspend/resume save and restore, and the GameSwitcher save path. Audio regression checks confirmed Gambatte remained at 32768 Hz source rate with RK817 output at 32000 Hz, exact-signature deep-resume recovery still triggered only for `17=06 38=00 40=69`, audio reopened successfully after resume, and frame pacing remained around the established ~59.7 FPS target with no resync storm or renewed audio-write backlog.

The State Manager owns state identity, storage, validation, recovery and atomic file transactions. Lifecycle policy remains outside the State Manager. On FAT32/vfat, FlipOS requires file-data `fsync()` before rename but does not claim journaled filesystem durability.

### Phase 5B — Save Data Protection

- [x] Battery-backed SRAM / `.sav` protection architecture
- [x] Atomic `.sav` handling
- [x] Backup/recovery policy for long-play save data
- [x] Validate core-specific save-data behavior before generalizing
- [x] Generic Core Adapter battery-memory access
- [x] Gambatte `RETRO_MEMORY_SAVE_RAM` bridge
- [x] Shared Save Manager path generation and exact-size validation
- [x] Protected `.sav.prev` previous-known-good copy
- [x] Invalid current `.sav` fallback to `.sav.prev`
- [x] Controlled lifecycle saves commit cartridge SRAM before emulator state
- [x] One-launch `--no-resume` path loads `.sav` while skipping `.state` restore
- [x] Normal launch loads `.sav` before automatic `.state` restore
- [x] START+SELECT, lid suspend/resume and GameSwitcher save paths preserve both `.sav` and `.state`
- [x] Full native regression suite and ARM64 static build pass
- [x] Real-Miyoo-Flip hardware validation passes

Hardware-tested code commit: `c716cda55c932b82a351155228ce490f77bbf448` (`Add boot without resume state option`). The exact tested `flip-gb` SHA-256 was `c65bab4b84d04dc7dddbac3167aa6deb7b5254cde67b95f5cba1f844ecd88722`.

Hardware acceptance used Pokemon Yellow on the real Miyoo Flip. Gambatte exposed `RETRO_MEMORY_SAVE_RAM` as exactly 32768 bytes, and a normal in-game Pokemon save produced a 32768-byte hashed `.sav`. A later protected commit created a 32768-byte `.sav.prev`. The `--no-resume` path loaded cartridge SRAM independently of emulator state, while the normal path logged `LOAD_CURRENT_OK` before `RESTORE_CURRENT_OK` and resumed the saved state normally. A deliberately truncated current `.sav` produced `LOAD_PREVIOUS_OK size=32768`; controlled exit then rebuilt a valid 32768-byte current save from the protected previous copy. Host backups confirmed the known-good cartridge-save SHA-256 `70c90f16530b74668488825d9eb8b3fe43293f2aa66c42a4102e715ecc889048` during the recovery test.

Lifecycle regression on the same candidate verified lid-close save, deep suspend/resume, START+SELECT exit and GameSwitcher request paths all committed both cartridge `.sav` and emulator `.state` successfully. GameSwitcher returned control to the frontend with `flip-gb` gone. Deep resume encountered the confirmed broken RK817 signature `17=06 38=00 40=69`; exact-signature recovery applied successfully, audio reopened at Gambatte 32768 Hz source -> RK817 32000 Hz output, and frame pacing returned to the established ~59.7 FPS target.

The Save Manager owns cartridge-save identity, storage, structural validation, protected-current/previous transactions and recovery. Emulator state and cartridge save remain separate: `.sav` is authoritative long-term cartridge progress, while `.state` is the automatic instant-resume snapshot. State restore does not itself write protected `.sav` data. FAT32/vfat protection relies on staged file writes, file-data `fsync()` and rename; FlipOS does not claim journaled filesystem durability.

### Phase 5C — Recovery Tooling / Policy

- [x] Save Safety / Recovery design for user-facing tooling
- [x] Core-aware Save Safety status and management commands
- [x] Generic frontend Resume Game / Start Normally / Save Safety actions
- [x] Non-destructive Restore Previous Save workflow
- [x] Paired pre-restore `.sav.recovery` / `.state.recovery` generation
- [x] Persistent `.restore-pending` resume block across restart/reboot
- [x] Fresh-state re-enable only after cartridge save + state both commit successfully
- [x] Undo Last Restore with paired-state and save-only safety behavior
- [x] Stale recovery-state generation rejection
- [x] Automatic lid-wake resume bypasses Game Options while deliberate launches remain interactive
- [x] Full native regression suite and ARM64 static build pass
- [x] Real-Miyoo-Flip hardware validation passes
- [ ] Multi-generation rotating backups, only if justified by later policy/testing

Hardware-tested code commit: `593eb53e54334fae67bd377b6f02bab107436ac2` (`Restore automatic lid resume through Save Safety`). Final tested binary SHA-256 values were `537eb2518739aa7ae947e56ce7aef5805845a58eda8d995373c2f48b194337cd` for `flip-gb` and `a114bcfa62bb14bb73420fd67da81d0352022b84320e95cc73c0abb1082a664f` for `flip-frontend`.

Hardware acceptance used Pokemon Yellow with distinct current/previous cartridge-save generations. Restore Previous activated the older progress, preserved the newer timeline as recovery data, quarantined the old resume state and kept Resume blocked until a fresh matching state was committed. Visual CONTINUE confirmed older restored progress; paired Undo visually returned to the newer progress. A no-current-state generation proved stale `.state.recovery` removal, and save-only Undo kept resume blocked when no matching recovery state existed. The recovery marker failed closed under deliberately destructive test interference.

Lifecycle regression verified Resume Game, lid suspend/resume, START+SELECT persistence, exact RK817 deep-resume recovery, Gambatte 32768 Hz -> RK817 32000 Hz audio conversion and established ~59.7 FPS timing. A Phase 5C wake regression that initially showed Game Options after lid-open was fixed with explicit one-shot automatic-resume session intent; hardware logs then showed `FRONTEND_STANDBY auto resume`, `FLIP_SAVE_SAFETY: AUTOMATIC_RESUME`, RK817 recovery and `RESTORE_CURRENT_OK`, while the user observed direct game resume with no menu.

Detailed evidence: `docs/superpowers/specs/2026-08-27-phase5c-hardware-acceptance.md`.

## Phase 6 — Global GameSwitcher / Session Manager

- [x] Suspend current game
- [x] Save state
- [x] Record active session
- [x] Switch games
- [x] Resume previous game
- [x] Close/remove sessions
- [x] Prune stale sessions
- [x] Recover interrupted sessions
- [x] Clean up exit-code protocol

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
- [x] Paths/storage service
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
