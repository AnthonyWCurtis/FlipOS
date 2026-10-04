# FlipOS Phase 7I Formal Logging Service Hardware Acceptance

Date: 2026-10-04
Device: Miyoo Flip, RK3566
ROM: `/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb`

## Candidate

Hardware-tested code commit: `8289b8b`

Deployed SHA-256:

```text
flip-frontend 0f9e82ad95497a8ca402ef211a5bdf47c83a259b9070a1950fd1f4eecf974038
flip-gb       974d73c315c6e981e7abc673d95cafa80929a01ac49cdb77efd5762f832244b4
```

The previously accepted Phase 7H binaries were preserved on-device as rollback copies before replacement.

## Host gate

`make verify` passed on the exact deployed tree.

Focused logging tests also passed:

- Logging format and severity filtering
- stream override
- canonical component log paths
- invalid-input handling
- Lifecycle Host integration
- frontend integration
- architecture guard

## Hardware acceptance

After one reboot:

- the new frontend started normally;
- global settings loaded as volume 5 / brightness 200;
- the existing suspended Pokemon Red session was discovered;
- the migrated frontend line remained compatibility-preserving:
  `FRONTEND_SESSION: suspended registry rom=...`;
- Pokemon Red launched under the new `flip-gb`;
- cartridge SRAM loaded successfully;
- the current emulator state restored successfully;
- `FLIP_CORE_HOST: resumed saved session` was logged;
- the Logging-backed Lifecycle Host emitted
  `FLIP_LIFECYCLE: timing resynced` on real hardware;
- runtime frame pacing remained around the established 59.7 FPS target.

Representative fresh evidence:

```text
FRONTEND_SESSION: suspended registry rom=/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb
FLIP_SAVE: LOAD_CURRENT_OK size=32768
FLIP_STATE: RESTORE_CURRENT_OK path=/mnt/SDCARD/flipos-dev/states/gb/f06ea3b0fd4ad958.state size=59650
FLIP_CORE_HOST: resumed saved session
FLIP_LIFECYCLE: timing resynced
```

Synthetic MENU and START+SELECT injection were not used as an acceptance requirement because the existing development injector does not reliably reach the emulator system-action layer. Repeating physical lid/MENU testing was intentionally avoided: Phase 7I changes log emission, not lifecycle decisions, and the unchanged lifecycle behavior remains covered by the focused Lifecycle Host tests plus successful real-hardware resume.

## Result

Phase 7I is accepted.

FlipOS now has a formal shared Logging service with severity policy, canonical log-path ownership, testable output routing and compatibility-preserving line format. Existing diagnostics can migrate incrementally without a risky bulk rewrite.
