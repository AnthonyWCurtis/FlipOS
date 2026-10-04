# Phase 7B — Formal Settings Service

Status: hardware accepted

## Goal

Formalize process-local ownership of FlipOS settings without changing the settings file format, defaults, persistence timing, or user-facing behavior.

## Design

- Settings file I/O remains owned by the Settings service.
- `flip_settings_init()` initializes process-local settings state once and is idempotent.
- `flip-frontend` initializes Settings once at frontend startup.
- the shared core host initializes Settings once for emulator processes.
- audio consumes `flip_settings_get_volume()` and no longer reloads the settings file itself.
- frontend consumes `flip_settings_get_brightness()`.
- hotkeys continue to update RAM-backed settings.
- the existing 750 ms delayed write remains unchanged.
- lifecycle shutdown continues to flush dirty settings.

Canonical file remains `/mnt/SDCARD/flipos-dev/state/settings/global`.

## Host validation

Passed:

- `make test-settings`
- `python3 tests/test_settings_architecture.py`
- `make test-core-host test-audio test-lifecycle`
- `make clean && make verify`

Behavioral coverage verifies first initialization, idempotent reinitialization, delayed persistence, and clean flush semantics.

## Hardware build

- flip-frontend: `2cc35b1ab68c3c5581426b39de2c82e24cf18c525c61e91c8ff19e47869fa6a5`
- flip-gb: `8de314aabb9fcc889abe8166be46103a6934d90e2859e169b672460913d255cc`

Phase 7A rollback copies were preserved before deployment.

## Hardware acceptance

Frontend startup loaded and initialized Settings exactly once at volume 5 and brightness 200.

Launching Pokémon Red created a new emulator process which independently initialized Settings exactly once. Audio then consumed global volume 5 without reloading the settings file.

Existing save/state resume remained intact: SRAM loaded at 32768 bytes and the existing Red state restored at 59650 bytes.

A real physical lid close/open cycle passed. Red suspended with exit code 20, deep sleep entered and resumed, automatic resume launched Red again, Settings loaded once in the resumed emulator process, the exact RK817 deep-resume signature `17=06 38=00 40=69` was repaired, and the existing state restored successfully.

After wake, the settings file remained volume 5 and brightness 200. Red returned to running with a new process id/start timestamp, and Blue remained independently recoverable.

## Result

Phase 7B is hardware accepted. The Formal Settings service now has explicit process-local initialization ownership while preserving existing persistence and runtime behavior.
