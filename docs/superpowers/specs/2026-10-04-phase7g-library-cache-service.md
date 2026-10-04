# Phase 7G — Library/cache Service

Status: hardware accepted

## Goal

Formalize Library/cache ownership of cache-generation policy without changing ROM discovery order, cache file formats, cache paths, or user-facing library behavior.

## Design

- add cache status classification: HIT, MISS, CHANGED, ERROR
- move fingerprint comparison and cache-generation validity policy into the Library service
- validate that the persisted index structurally matches the live ROM count before reporting HIT
- move index + fingerprint commit sequencing into the Library service
- keep frontend ownership limited to UI-facing ROM entry construction and OSD presentation
- preserve existing version=1 fingerprint/index formats and canonical cache paths

## Host validation

Passed:

- `make test-library-cache`
- Library/cache architecture guard
- global verify updated so all formal-service behavioral tests run
- full `make clean && make verify`
- `git diff --check`

Final host result: `VERIFY: PASS`.

## Hardware build

- flip-frontend: `36b3a39c1d8d5a6e0793b6c4b231fe68aa4078d80e1aad9c24cb073324c54262`
- flip-gb unchanged from Phase 7F: `cb3acf6608adddc1bb527d3856d4d61042006a76f413f5cffa2ee4765c261eea`

Phase 7F frontend rollback was preserved before deployment.

## Hardware acceptance

Phase 7G booted successfully on the exact staged frontend hash.

Persisted settings remained:

- volume=5
- brightness=200

Existing Red and Blue session metadata remained intact.

### GB existing cache

GB reported a clean cache HIT with 2 ROMs.

### GBC cache generation

GBC initially reported MISS with 4 ROMs.

Opening GBC through the real frontend produced exactly one rebuild:

- index saved with 4 ROMs
- fingerprint saved with 4 ROMs
- generation committed successfully
- ROM list was Crystal, Gold, Silver, Yellow in the expected sorted order

Backing out and reopening GBC then produced:

- cache HIT
- persisted index loaded
- 4 ROMs loaded from cache
- no rebuild

### Cache path/format preservation

Files remained at the canonical paths under:

`/mnt/SDCARD/flipos-dev/cache/library`

The GBC fingerprint remained `version=1` with:

- fingerprint=33ffc83394fc039e
- rom_count=4

The GBC index remained `version=1` with the same four ordered filenames.

No cache migration or duplicate cache tree was created.

### Gameplay regression

Red resumed through the normal frontend path:

- existing battery save loaded at 32768 bytes
- existing state restored at 59650 bytes
- session returned to running

### Real lid close/open

After `PHASE7G_LID_TEST_START`, a physical lid close/open cycle produced:

- FLIP_LIFECYCLE: lid closed
- FLIP_LIFECYCLE: suspend requested
- save write succeeded
- state write succeeded
- child exited with code 20 / suspend action
- FRONTEND_STANDBY lid closed after suspend
- deep sleep entered
- device resumed and eventually observed lid open
- automatic resume relaunched Red
- RK817 deep-resume recovery was required and applied
- existing 59650-byte state restored
- Red returned to running

While the lid remained closed, the existing power loop may re-enter suspend after an early wake; it ultimately remained in the proven standby flow until the hall sensor reported open.

### Final MENU return

After resume, physical MENU produced:

- save write succeeded
- state write succeeded
- child exited with code 20 / suspend action
- FRONTEND_GAMESWITCHER requested
- current session became suspended

No `flip-gb` process remained afterward, leaving the Flip quiet at GameSwitcher.

## Result

Phase 7G is hardware accepted.

Library/cache now owns complete cache-generation policy and commit sequencing while preserving existing ROM ordering, paths, file formats, sessions, UI behavior, and suspend/resume behavior.
