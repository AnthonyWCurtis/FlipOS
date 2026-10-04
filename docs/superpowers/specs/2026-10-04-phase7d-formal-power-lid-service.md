# Phase 7D — Formal Power/Lid Service

Status: hardware accepted

## Goal

Formalize Power ownership of lid-state interpretation and standby entry without changing the proven hall sensor, deep-suspend, backlight, or automatic-resume behavior.

## Design

- Input remains the authoritative hardware owner of hall state.
- Power classifies refreshed lid state as error, unknown, open, or closed.
- frontend post-session standby decisions go through Power rather than interpreting raw lid fields directly.
- the private frontend wait-for-lid-open wrapper is removed.
- the proven RK3566 deep-suspend loop remains unchanged.
- brightness clamping remains 10-255.
- power hardware paths are host-overridable for deterministic tests.

## Host validation

Passed:

- `make test-power`
- `python3 tests/test_power_architecture.py`
- `make clean && make verify`

Behavioral coverage verifies:

- brightness readback
- brightness lower clamp to 10
- brightness upper clamp to 255
- lid poll error classification
- unknown lid classification
- open lid classification
- closed lid classification

## Hardware build

- flip-frontend: `02e0e4efa18ae9bf50751796994305ba24ca81c4b7d0bdaa9cac285ccb77b9a2`
- flip-gb: `330854ea703364b10ebe9eedfcda3fed415dad9ed64d5b75ecb2728125e2f1fe`

Phase 7C rollback copies were preserved before deployment.

## Hardware acceptance

Phase 7D booted successfully on the exact staged hashes.

Persisted settings remained:

- volume=5
- brightness=200

Existing Red and Blue session metadata remained readable.

### MENU with lid open

With Pokémon Red running, a physical MENU press returned to GameSwitcher normally and did not enter standby.

The current hardware-test log contains the corresponding latest:

- session suspend / GameSwitcher action
- FRONTEND_GAMESWITCHER requested

This proves an open lid still routes suspend action 20 to GameSwitcher rather than the standby path.

### Real lid close/open

After resuming Red, a physical lid close/open cycle produced:

- FLIP_LIFECYCLE: lid closed
- FLIP_LIFECYCLE: suspend requested
- save and state writes succeeded
- SESSION child exited code=20 action=2
- session state changed to suspended
- FRONTEND_STANDBY lid closed after suspend
- POWER: deep sleep enter brightness=200
- POWER: suspending
- POWER: resumed
- POWER: lid open
- POWER: deep sleep exit brightness=200
- FRONTEND_STANDBY auto resume
- automatic resume relaunched Red
- settings reloaded at volume=5 brightness=200
- RK817 signature 17=06 38=00 40=69 was repaired
- existing 59650-byte state restored
- Red returned to running with a new process id/start timestamp
- Blue remained independently recoverable

## Result

Phase 7D is hardware accepted.

The Formal Power/lid service now owns lid-state interpretation and standby entry while preserving the proven hardware sensing, deep-suspend, wake, audio recovery, and automatic-resume behavior.
