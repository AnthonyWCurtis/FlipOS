# Phase 7C — Formal OSD Service

Status: hardware accepted

## Goal

Formalize process-local OSD ownership without changing the existing FlipOS OSD appearance or user-facing behavior.

## Design

- add explicit `flip_osd_init()` and `flip_osd_reset()` lifecycle
- add `flip_osd_snapshot()` for deterministic service-state inspection
- preserve the existing 1-second transient duration
- preserve volume, brightness, status and power-off OSD behavior
- preserve power-off priority over transient feedback
- initialize OSD once in frontend and emulator process owners
- keep rendering layout and visuals unchanged

## Host validation

Passed:

- `make test-osd`
- `python3 tests/test_osd_architecture.py`
- `make clean && make verify`

Behavioral coverage verifies:

- reset/init clears service state
- volume percent clamps to 0-100
- brightness maps 10 -> 0% and 255 -> 100%
- status text is copied into service state
- power-off progress takes priority
- clearing power-off progress reveals the still-valid transient status
- transient OSD expires after one second

## Hardware build

- flip-frontend: `4fad742f3ca654a3a89bcaca6378a3f85982f9980561210b306d4f03e2941b5b`
- flip-gb: `3d95ebd9817d22c31dea87df620caeb96d044cb8af0b74f82f80f6c26e164400`

Phase 7B rollback copies were preserved before deployment.

## Hardware acceptance

Phase 7C booted successfully on the exact staged hashes. Existing settings and sessions were preserved.

Pokémon Red resumed from its existing save/state:

- SRAM load size: 32768 bytes
- state restore size: 59650 bytes

Physical display acceptance:

- Volume + showed the normal VOLUME OSD.
- Volume - restored the original volume.
- MENU + Volume + showed the normal BRIGHTNESS OSD.
- MENU + Volume - restored the original brightness.
- both OSDs looked unchanged on the physical display.

Settings after the visual checks remained:

- volume=5
- brightness=200

A real lid close/open cycle passed:

- Red saved and suspended with exit code 20.
- frontend entered deep sleep and resumed.
- automatic resume relaunched Red.
- Settings reinitialized at volume 5 / brightness 200.
- RK817 deep-resume signature 17=06 38=00 40=69 was repaired.
- the existing state restored successfully.
- Red returned to running with a new process id/start timestamp.
- Blue remained independently recoverable.

## Result

Phase 7C is hardware accepted. The Formal OSD service now has an explicit lifecycle and deterministic state contract while preserving the existing OSD visuals and runtime behavior.
