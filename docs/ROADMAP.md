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

- [x] Formal Settings service
- [x] Formal OSD service
- [x] Formal Power/lid service
- [x] Save Manager
- [x] State Manager
- [x] Library/cache service
- [x] BIOS manager
- [x] Logging
- [x] Battery service
- [x] Performance profiles
- [x] Paths/storage service
- [x] Session persistence

## Phase 8 — Second Emulator / Architecture Proof

Preferred system: Game Boy Advance with mGBA.

- [x] Flip_GBA executable
- [x] Reuse same Video Host
- [x] Reuse same Audio Host
- [x] Reuse same Input Host
- [x] Reuse same Timing Host
- [x] Reuse same Lifecycle Host
- [x] Reuse same State/Save Managers
- [x] Reuse same GameSwitcher
- [x] Thin mGBA adapter only

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

### Phase 10A — Measurement / Characterization

- [x] Persistent playtest telemetry
- [x] Stock GB/GBC/GBA performance baseline
- [x] RK3566 CPU OPP / regulator reconnaissance
- [x] GBA frequency-ceiling characterization across LeafGreen, Golden Sun: The Lost Age, Mother 3 and Final Fantasy VI Advance
- [x] Establish 1.104 GHz as the conservative GBA starting ceiling on the current hardware
- [x] Establish that 816 MHz is not a safe general GBA ceiling (Final Fantasy VI slowdown)
- [x] Complete controlled unplugged A/B battery measurements

### Phase 10B — FlipOS Dynamic CPU Performance Controller

Architecture rule: FlipOS manages a safe CPU-frequency ceiling while the kernel
ondemand governor remains responsible for normal down-clocking within that
ceiling. Emulator performance, not a hard-coded per-game database, can raise
the ceiling one valid RK3566 OPP at a time when more headroom is required.

- [x] Native cpufreq policy service
- [x] Capture and restore the exact pre-launch CPU policy
- [x] Never raise a lower limit imposed by another safety/thermal policy
- [x] Dynamic ceiling escalation state machine with host tests
- [x] Apply managed GBA starting ceiling automatically at session launch
- [x] Feed emulator FPS/frame-time/audio-pressure windows into the controller
- [x] Raise ceiling one OPP on sustained or severe performance pressure
- [x] Never exceed the pre-launch maximum ceiling
- [ ] Restore exact pre-launch policy after normal exit, GameSwitcher, error or crash (normal GameSwitcher hardware path verified; error/crash hardware path pending)
- [x] Verify lid sleep/wake reapplies and restores the managed policy correctly
- [x] Real-hardware GBA regression across the four characterization titles
- [x] Characterize GB/GBC starting ceilings and enable the same dynamic controller
- [ ] Consider persistent per-title learned ceilings only if later evidence justifies it

Initial policy is intentionally upward-only within a session. This avoids
frequency oscillation and avoids deliberately degrading gameplay to probe a
lower ceiling. The kernel governor still selects lower clocks automatically
when workload permits.

Current hardware acceptance evidence: Final Fantasy VI Advance launched through
the FlipOS frontend with the pre-launch 1.992 GHz ceiling captured, a managed
1.104 GHz ceiling applied, and the dynamic controller enabled with 1.992 GHz as
its absolute escalation limit. Sustained ~59.73 FPS caused no unnecessary
escalation. A GameSwitcher suspend committed cartridge save + state, terminated
the GBA runner normally and restored the exact 1.992 GHz pre-launch ceiling
before frontend hardware ownership resumed.

Lid sleep/wake was also verified on real hardware. Closing the lid caused FFVI
to commit save + state, exit through the normal suspend path and restore the
1.992 GHz pre-launch ceiling before deep sleep. On wake, the frontend
automatically resumed FFVI, reapplied the managed 1.104 GHz ceiling, re-enabled
the dynamic controller with 1.992 GHz as its escalation limit and restored the
430144-byte FFVI savestate successfully. Post-wake FPS remained at the ~59.73
target with no unnecessary clock escalation.

Dynamic upward rescue was then exercised directly on hardware with FFVI from an
816 MHz developer start. The first implementation correctly raised 816 -> 1104
MHz under severe slowdown, but hardware testing exposed a false-escalation bug:
near-frame-budget host time could continue raising clocks even after FPS had
fully recovered. The controller was corrected so host-time pressure is only
supporting evidence when measured FPS is already slipping; healthy full-speed
FPS is authoritative.

The corrected controller was re-tested on the same Flip. FFVI initially held
near target at 816 MHz and correctly stayed there. When workload increased, FPS
fell through 58.595 to 57.820, FlipOS raised the ceiling exactly one OPP from
816 -> 1104 MHz, and FPS immediately recovered to ~59.72-59.73. The controller
then remained at 1104 MHz across many subsequent full-speed windows with no
false escalation to 1416 MHz. This validates both performance rescue and
post-recovery hysteresis on real hardware.

A short production-profile regression then ran the same dynamic controller at
its normal 1104 MHz GBA starting ceiling across LeafGreen, Golden Sun: The Lost
Age, Mother 3 and Final Fantasy VI Advance. Each title completed the controlled
hardware window with zero clock raises and final measured FPS of 59.724,
59.723, 59.727 and 59.726 respectively. The harness restored the stock 1992 MHz
frontend ceiling afterward. This closes the initial four-title false-escalation
acceptance while longer real-world soak remains part of ongoing playtesting.

GB/GBC characterization then pinned Gambatte at the RK3566 minimum 408 MHz OPP.
Pokemon Crystal completed its controlled run with min/avg/max FPS of
59.713/59.727/59.742, zero audio-drop windows and zero timing resyncs. Pokemon
Blue completed a longer 45-second follow-up with 44 measured windows at
59.715/59.727/59.741, again with zero audio drops or resyncs. Because the
available GB/GBC ROM set is currently Pokemon-only, 408 MHz is treated as a
provisional starting ceiling rather than a universal fixed requirement. GB and
GBC now use the same upward-rescue controller so a future heavier Gambatte
workload can automatically request more CPU headroom.

### Phase 10C — CPU Optimizer / Voltage Calibration

- [x] Native optimizer result/state foundation
- [x] RK3566 voltage/OPP reconnaissance
- [x] Watchdog/rollback proof before voltage writes
- [x] Per-device silicon calibration framework

#### Phase 10C1 — FlipOS Self-Healing Calibration Supervisor

This is now a hard safety gate before any further live undervolt sweep. FlipOS
must own the complete failure/recovery loop so a marginal voltage can freeze or
reset the RK3566 without requiring manual ADB intervention or trusting the
unstable userspace test process to write a final result.

- [x] Build a native FlipOS recovery supervisor independent of the emulator/frontend
- [x] Arm the recovery path before every voltage write and verify it before entering the candidate OPP
- [x] Keep dangerous-phase coordination in RAM; avoid SD/FAT writes while silicon is intentionally unstable
  - [x] Quiesce playtest/frontend SD writers before synthetic live candidate entry and restore them after stock voltage returns
- [x] Persist only the minimum pre-trial durable record before the test, sync it once, then stop writing until stock voltage is restored
- [x] On reboot, treat an armed-but-unconfirmed trial as failed and restore stock-only operation before normal FlipOS startup
- [x] Distinguish clean pass, controlled failure, watchdog/reset, power loss and corrupt/incomplete trial state
- [x] Prove recovery still works if the SD card remounts read-only after a hard crash
- [x] Investigate and validate an RK3566 full-board reset path; use hardware watchdog only if it can autonomously return this board to boot
- [x] Add a RAM-resident high-priority deadman fallback using direct reboot/reset primitives where the kernel permits
- [x] Add kernel panic/oops/stall auto-reboot policy only after hardware validation on this exact Flip
- [x] Resume calibration automatically at the correct candidate after a recovered failure
- [ ] Never publish or persist an optimized profile unless every accepted OPP has completed the full validation matrix

Implementation progress on 2026-10-07: the native `flip-optimizer-supervisor`
now performs the boot safety decision before any SD-card write, enforces
stock-safe quarantine through a `/tmp` RAM latch, and allows durable rollback
settlement to be deferred when FAT is read-only. The live candidate runner
refuses voltage work while that RAM latch is present. A separate
`flip-optimizer-deadman` is loaded and armed before candidate voltage entry,
locks itself into RAM, raises itself to real-time SCHED_FIFO priority and uses
a storage-free direct restart syscall if its deadline expires. Controlled
candidate failures that successfully return to stock voltage are now committed
as unstable without rebooting; attempts that fail before candidate voltage was
actually entered are cancelled rather than misclassified as silicon failures.

The RK3566 kernel exposes `/sys/kernel/boot_mode` through its existing
syscon-reboot-mode driver. Hardware acceptance on this physical Flip proved
both unattended recovery layers: the RAM-locked deadman returned with
`boot_mode=watchdog`, while a controlled SysRq kernel fault returned with
`boot_mode=panic` after the temporary 10-second kernel panic timeout. Ramoops
captured persistent panic evidence, the SD returned read-write, FlipOS frontend
and telemetry restarted, and the temporary kernel recovery sysctls returned to
their original zero values. Two-phase PMUGRF trial state also distinguishes a
prepared-but-unentered candidate from a candidate whose dangerous window was
actually entered, preventing unrelated warm reboots from falsely condemning a
voltage.

#### Phase 10C2 — FlipOS Per-Device Voltage Characterization

- [ ] Keep 408/600/816 MHz at stock voltage unless Flip-specific evidence proves low-load undervolting is safe and worthwhile
- [ ] Continue descending higher OPPs in 12.5 mV steps, highest frequency first
- [ ] Find each raw stability boundary, then apply the existing 25 mV production guardband
- [ ] Validate static heavy load
- [ ] Validate idle/light-load residency
- [ ] Validate rapid DVFS transition stress across the full OPP ladder
- [ ] Validate real emulators and dynamic-clock escalation/de-escalation behaviour
- [ ] Validate thermal variation, sleep/resume and long soak
- [ ] Treat reboot, freeze, USB collapse, regulator fault, kernel fault or corrupted trial state as candidate failure
- [ ] Apply stock fallback whenever calibration evidence is incomplete or suspect
- [ ] Optimizer results UI with measured whole-device A/B power data
- [ ] Undervolting only when hardware support and self-healing rollback safety are verified

Per-device calibration framework acceptance on 2026-10-06 is also complete.
The read-only live probe identifies this physical Flip by RK3566 CPU serial
b9f4ffca2f298ad2 and model MIYOO RK3566 355 V10 Board, reads all eight CPU
OPPs from debugfs, and reads the vdd_cpu regulator floor directly from sysfs.
The resulting plan uses 12.5 mV candidate steps and a 25 mV final guardband,
tests highest OPP first, persists the exact pending OPP/voltage/trial ID, and
can map a boot rollback back to that candidate as unstable.

The initialized hardware plan contains all eight stock OPPs from 408 MHz /
850 mV through 1992 MHz / 1100 mV with the RK860 regulator floor at 712.5 mV and an effective CPU calibration floor of 812.5 mV.

The stronger transaction path was also dry-run on hardware. The first 1992 MHz
candidate (1087.5 mV) was persisted and paired with a matching boot-guard trial,
which made ready_for_apply=1 while the live kernel OPP remained at the stock
1100 mV. Rebooting without confirmation caused the guard to roll back before
the frontend and the boot reconciler mapped that exact trial back to the
pending calibration candidate. The artificial result was then discarded, the
stock-only latch cleared, and a clean plan reinitialized with pending=0 and
complete_opps=0. No voltage was written during this milestone.

Hardware rollback acceptance on 2026-10-06 used a fake optimizer trial with
no voltage writes. The trial marker was durably armed and the Flip was rebooted
without confirmation. On the following boot, flip-optimizer-guard ran before
the playtest logger and frontend, detected the unconfirmed trial, cleared the
active marker, recorded last_action=rolled_back, set the durable stock_only=1
latch and allowed the device to continue on the normal stock 408-1992 MHz
ondemand policy. A complementary confirmed-trial reboot preserved
last_action=confirmed, kept stock_only=0 and produced a normal safe boot. Host
tests also cover corrupt-marker recovery and block automatic retry while the
stock-only latch is set.

The safety contract intentionally assumes future experimental voltage changes
are runtime-only and non-persistent: reboot itself returns the kernel to its
stock device-tree OPP table, while the guard prevents the failed candidate from
being retried automatically. Persistent boot-time OPP/DTB experiments remain
prohibited until an earlier boot-stage fallback path is independently proven.

The runtime kernel OPP update path is now hardware-proven using the matching
Rockchip 5.10.160 OPP API. The first real 1992 MHz / 1087.5 mV candidate was
also accepted in table-only mode: CPU0 was parked at 1800 MHz / 1050 mV, the
candidate table entry was applied and verified without entering that OPP,
then the stock table and exact pre-test CPU policy were restored and the
unapplied trial cancelled.

The RK3566 DesignWare watchdog was also exercised with a stopped feeder. It
fired and removed the device from USB, but did not return this Miyoo board to a
normal autonomous boot; manual power-cycle was required. After manual boot the
durable guard correctly quarantined the unconfirmed trial and the stock kernel
OPP table was restored. Until a true full-board unattended reset mechanism is
proven, hard-hang recovery during runtime-only undervolt calibration may require
manual power-cycle.

### Phase 10D — Thermal / Charging / Runtime Efficiency

- [ ] Smart charge-while-playing policy
- [ ] Suspend optimization
- [ ] Battery drain investigation
- [ ] Thermal behavior
- [ ] Frame pacing optimization
- [ ] Audio latency tuning
- [ ] Idle behavior

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
