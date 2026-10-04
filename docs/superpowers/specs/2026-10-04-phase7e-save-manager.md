# Phase 7E — Save Manager

Status: hardware accepted

## Goal

Formalize Save Manager ownership of aggregate save health without changing save paths, save file formats, transaction semantics, recovery behavior, or user-facing Save Safety behavior.

## Design

- add `flip_save_status_t` as the manager-owned aggregate health contract
- add `flip_save_query_status()` to report current, previous and recovery health in one call
- make `flip_gb` Save Safety management consume the aggregate manager status
- forbid direct per-candidate `flip_save_health()` ownership in `flip_gb`
- preserve transactional temp writes, previous generation handling, recovery/undo behavior and frozen ROM identity

Canonical save paths remain unchanged under `/mnt/SDCARD/flipos-dev/saves`.

## Host validation

Passed:

- `make test-save-manager`
- Save Manager architecture guard
- existing `make test-save test-recovery`
- full `make clean && make verify`

Full verification result: `VERIFY: PASS`.

## Hardware build

- flip-frontend: `eb4479de5237f21937dcc31589b44d5ad4d1a991946b45ddc1eb7ace9ac80b29`
- flip-gb: `7d4f1a54ed04b75c0bc5692045cecd8a71b759fb070bd5ac4d1b7b3e48b814b3`

Phase 7D rollback binaries were preserved before deployment.

## Hardware acceptance

Phase 7E booted successfully on the exact staged hashes.

Persisted settings remained:

- volume=5
- brightness=200

Red and Blue session metadata remained intact.

### Aggregate Save Manager status

Running:

`flip-gb <Red ROM> --save-safety-status`

produced:

- current=HEALTHY
- previous=HEALTHY
- recovery=MISSING
- resume=AVAILABLE
- undo_state=MISSING

This exercises the new aggregate Save Manager contract against Red's real files.

### Existing save/state resume

Red resumed through the normal frontend path:

- current battery save loaded successfully at 32768 bytes
- existing state restored successfully at 59650 bytes
- session state returned to running
- settings/audio behavior remained unchanged

### MENU suspend

Using the real event0 path with a 500 ms synthetic MENU hold:

- save commit succeeded to the existing .sav path
- state save succeeded to the existing .state path
- child exited with code 20 / suspend action
- session changed to suspended
- frontend returned to GameSwitcher

The longer hold makes automated MENU testing reliable, and the Flip was left quiet at GameSwitcher after acceptance.

### Real lid close/open

A physical lid close/open cycle produced:

- FLIP_LIFECYCLE: lid closed
- FLIP_LIFECYCLE: suspend requested
- save and state writes succeeded
- child exited with code 20 / suspend action
- FRONTEND_STANDBY lid closed after suspend
- deep sleep entered and resumed
- hall state returned open
- automatic resume relaunched Red
- existing battery save loaded
- RK817 signature 17=06 38=00 40=69 was detected and recovered
- existing 59650-byte state restored
- Red returned to running with a new PID/start timestamp
- Blue remained independently recoverable

Red was then returned to GameSwitcher after the test.

## Result

Phase 7E is hardware accepted.

The Save Manager now owns aggregate save-health status while preserving all existing save transaction, recovery, persistence, path and UI behavior.
