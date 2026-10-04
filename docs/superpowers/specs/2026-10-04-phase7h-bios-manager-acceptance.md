# FlipOS Phase 7H BIOS Manager Hardware Acceptance

Date: 2026-10-04
Device: Miyoo Flip, RK3566
ROM used: `/mnt/SDCARD/Roms/GB/Pokemon - Red Version (USA, Europe).gb`

## Candidate

Hardware-tested head: `ce3dafb89f74eca1547f35025acc52fa2914ef84`

Final deployed binary SHA-256 values:

```text
flip-frontend 134f84ea0536471d1d65a97161c5d4e06abcef128eee9bd777f20ad98b1aad39
flip-gb       1c0c7b15822c8643b66c3fe9af10dc27ffb64ce7f202138661e536cc47de68d3
```

## BIOS Manager acceptance

Real-device testing proved:

- the canonical GB system directory is `/mnt/SDCARD/flipos-dev/system/gb`;
- with no BIOS files present, DMG and CGB both report `MISSING` and game startup remains allowed;
- user-supplied `gb_bios.bin` at exactly 256 bytes reports `VALID`;
- user-supplied `gbc_bios.bin` at exactly 2304 bytes reports `VALID`;
- copied test BIOS files preserved their source SHA-256 values on the SD card;
- temporary BIOS test copies were removed after validation, returning the device to the intended `MISSING / MISSING` baseline;
- Gambatte still does not enable `gambatte_gb_bootloader`;
- no BIOS bootloader-read behavior was observed.

The BIOS Manager contract for Phase 7H is structural exact-size validation only. It does not authenticate firmware content and it does not ship, download, generate, or enable BIOS files.

## Gameplay and persistence regression

Pokemon Red regression testing on the same hardware candidate proved:

- existing battery save data loads successfully;
- current emulator state restores successfully;
- MENU/GameSwitcher suspension saves both `.sav` and `.state`;
- the session registry returns to `suspended` state correctly;
- no regression was observed in existing save/state lifecycle behavior.

## Physical lid regression

The final physical close/open test produced the complete expected lifecycle:

```text
FLIP_LIFECYCLE: lid closed
FLIP_LIFECYCLE: suspend requested
FLIP_SAVE: SAVE_OK
FLIP_STATE: SAVE_OK
FRONTEND_STANDBY lid closed after suspend
POWER: deep sleep enter brightness=200
POWER: suspending
POWER: resumed
POWER: lid open
FRONTEND_STANDBY auto resume
FLIP_SAVE_SAFETY: AUTOMATIC_RESUME
FLIP_SAVE: LOAD_CURRENT_OK
flip_audio: RK817 deep-resume recovery required
flip_audio: RK817 deep-resume recovery applied
flip_audio: RK817 audio recovered
FLIP_STATE: RESTORE_CURRENT_OK
FLIP_CORE_HOST: resumed saved session
FLIP_LIFECYCLE: lid open
```

The user then pressed physical MENU. FlipOS saved both cartridge data and emulator state again, exited cleanly with action 2, returned to GameSwitcher, and left Pokemon Red suspended.

## Final device state

- FlipOS is at GameSwitcher.
- Pokemon Red is suspended with current save/state committed.
- Global settings restored to volume 5 and brightness 200.
- BIOS test copies are absent from the SD card.
- Phase 7G accepted binaries remain available as rollback backups.

## Result

Phase 7H BIOS Manager hardware acceptance is complete. The service owns canonical GB/GBC BIOS metadata and structural health reporting while preserving existing Gambatte runtime behavior and leaving BIOS execution disabled.
