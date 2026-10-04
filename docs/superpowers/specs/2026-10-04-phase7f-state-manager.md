# Phase 7F — State Manager

Status: hardware accepted

## Goal

Formalize State Manager ownership of aggregate state-generation health without changing state paths, state file formats, save/restore transactions, recovery behavior, or user-visible Save Safety behavior.

## Design

- add generic state health values for current, previous, and recovery generations
- add `flip_state_status_t`
- add `flip_state_query_status()`
- preserve `flip_state_recovery_health()` as a compatibility wrapper
- make Save Safety consume aggregate State Manager status instead of calling recovery health directly
- preserve existing atomic state writes, previous-generation preservation, recovery snapshots, quarantine, restore, and frozen ROM identity

Canonical state paths remain unchanged under `/mnt/SDCARD/flipos-dev/states`.

## Host validation

Passed:

- `make test-state-manager test-state test-state-recovery-health`
- State Manager architecture guard
- Save Safety management architecture guard
- full `make clean && make verify`

Full verification result: `VERIFY: PASS`.

## Hardware build

- flip-frontend: `1d5d6b4a2927d5d3bbf2d0a0a5d99aa0b17bf4b67cb7298f2a090bba1022db32`
- flip-gb: `cb3acf6608adddc1bb527d3856d4d61042006a76f413f5cffa2ee4765c261eea`

Phase 7E rollback binaries were preserved before deployment.

## Hardware acceptance

Phase 7F booted successfully on the exact staged hashes.

Persisted settings remained:

- volume=5
- brightness=200

Existing Red and Blue session metadata remained intact.

### Aggregate State Manager status

The Save Safety management command completed successfully through the new aggregate State Manager path and reported:

- current save=HEALTHY
- previous save=HEALTHY
- recovery save=MISSING
- resume=AVAILABLE
- undo_state=MISSING

### Existing state restore

Red resumed through the normal frontend path:

- current battery save loaded at 32768 bytes
- existing state restored at 59650 bytes
- session returned to running
- audio/settings behavior remained unchanged

### Real lid close/open

A physical lid close/open cycle after `PHASE7F_LID_TEST_START` produced:

- FLIP_LIFECYCLE: lid closed
- FLIP_LIFECYCLE: suspend requested
- save write succeeded
- state write succeeded
- child exited with code 20 / suspend action
- FRONTEND_STANDBY lid closed after suspend
- POWER deep sleep entered
- POWER suspended and resumed
- hall state returned open
- automatic resume relaunched Red
- RK817 deep-resume recovery was required and applied
- current 59650-byte state restored successfully
- Red returned to running

### Final MENU return

After resume, a physical MENU press produced:

- save write succeeded
- state write succeeded
- child exited with code 20 / suspend action
- FRONTEND_GAMESWITCHER requested
- current session state became suspended

No `flip-gb` process remained afterward, leaving the Flip quiet at GameSwitcher.

## Result

Phase 7F is hardware accepted.

State Manager now owns aggregate current/previous/recovery state health while preserving all existing state transaction, recovery, path, resume and UI behavior.
