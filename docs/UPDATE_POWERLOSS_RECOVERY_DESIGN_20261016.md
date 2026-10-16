# FlipOS interrupted-update boot recovery — engineering design

**Date:** 16 October 2026  
**Source baseline:** `AnthonyWCurtis/FlipOS` `main` at `6d6eb28e93e1eee24cb9e0b23b64d3641c6bee19`  
**Status:** DESIGN + HOST-ONLY MODEL, **NOT A PRODUCTION RECOVERY IMPLEMENTATION**  
**Related unmerged drafts:** #43 (recovery resume-block ordering) and #44 (pre-recorded updater rollback ledger)

## 1. Findings in the published source

`tools/update/flip-update-install` validates an incoming update, stages it, verifies staged hashes, recreates `.update-rollback/current`, copies active files into backup, then replaces files one-by-one with `${dest}.new` and `mv`. It writes `state/update/last-good` only after the file loop. As published on `main`, the rollback `installed.list` is appended *after* each replacement; draft #44 changes this to an up-front complete ledger before any replacement. `trap cleanup` removes the stage and update lock on normal shell exit or handled signals. It cannot run after a power cut or `SIGKILL`.

`tools/device/updater-playtest` starts early at SD boot, runs the stock-safe CPU optimiser boot check before writing to SD, then sets up logging, charge/USB helpers, and eventually starts FlipOS. The inspected script does **not** detect an unfinished update transaction before starting potentially mixed-version runtime binaries. The boot-hook file itself is updated during installation, so blindly adding a recovery command inside a replaceable binary or script is not a sufficient trusted-bootstrap design.

These are **source-derived conclusions**, not evidence of an observed Miyoo Flip failure. Changes present only on the MSI (not yet pushed) may supersede this baseline.

## 2. Safety boundary and non-goals

- Protect ROMs, cartridge `.sav`, save states, RTC, game artwork, settings and stock recovery. An update rollback may alter only allowlisted runtime release files.
- Do not touch NAND/MTD, voltage, CPU optimisation, the real handheld, or existing user saves during host testing.
- The code in `tests/models/` is an **executable safety oracle** for a proposed protocol. It neither patches the production updater nor guarantees SD/FAT32 durability.
- Never describe a Unix host rename or host `fsync` test as proof of successful recovery after loss of power on FAT32/vfat.
- A stock boot fallback must be positively proven. A shell `exit` or black screen is not an accepted recovery mechanism.

## 3. Required invariants

**U1 — Backups are complete before live mutation.** Write a complete allowlisted file ledger and store verified before-images, including explicit “previously absent” entries. Protect at least the active rollback generation while constructing a new candidate; do not destroy the sole known-good rollback copy.

**U2 — Journal published before live mutation.** Create a versioned, bounded, syntactically validated transaction manifest containing target/device identity, version, allowed paths and old/new hashes. Publish a `pending` indicator only after all required backups and staged files pass verification and have received the appropriate write barriers.

**U3 — Startup authority.** Before launching any possibly updated frontend, GPU host, emulator or updater helper, a trusted, stable boot component must reconcile or reject an unfinished transaction. This component cannot depend solely on a binary or script that may itself have been replaced incompletely. Establish a verified stock-safe path before changing the boot hook.

**U4 — Pending means old-or-safe-failure.** On `pending`, verify **all** immutable backup hashes and ledger entries before changing any live file. Restore by copying each backup into a staging name then replacing it. Keep the source backups and marker intact until the entire old set has been reverified. Previously absent files must be removed as appropriate. Recovery must be idempotent across repeated crashes during rollback.

**U5 — Commit is explicit and verifiable.** Mark `committed` only after the entire new set, including any boot hook updates, has passed manifest/hash verification. On reboot after a committed marker, independently revalidate the new set. Treat any missing, malformed or mismatched marker/manifest as unsafe; do not silently launch.

**U6 — Power loss and read-only media fail closed.** Missing backups, torn journals, unexpected symlinks and unwritable SD media must leave existing evidence intact and use a tested safe fallback. Never erase a marker merely to let the device continue booting.

**U7 — Rollback freshness and concurrency.** A stale lock directory does not prove another updater is running. A new update must not overwrite unresolved journal evidence; concurrency and crash leftovers are distinct.

**U8 — Saves remain outside the updater transaction.** The save/RTC/state recovery design must separately address multi-file consistency. Safe updater recovery alone does not fix gameplay save transactions.

## 4. Proposed boot transaction states

| State | Stable evidence | Boot decision |
| --- | --- | --- |
| Clean | No pending or unresolved transaction | Normal boot after independent runtime integrity checks |
| Preparing | Stage/backup being built, active runtime still untouched | Clean obsolete staging **only if provenance and absence of live mutation are provable** |
| Pending | Verified manifest + old backup set published before first live write | Validate backups, restore entire old set, verify, then clear pending |
| Rolling back | Same pending journal; some old files may be restored | Repeat the idempotent full restore from preserved backups |
| Committed | All new runtime file hashes verified, commit evidence published | Revalidate new generation, then launch; otherwise safe fallback |
| Invalid/unknown | Missing/torn journal, backup checksum mismatch, untrusted boot helper, RO SD | Block FlipOS runtime and enter **hardware-tested** safe recovery path |

For actual implementation, a single transaction indicator should be designed so torn SD writes cannot be misread as a clean state; use redundant/versioned evidence or a slot-pointer protocol if required by measured FAT32 behaviour. Consider an immutable/stable bootstrap with A/B runtime slots as a safer long-term alternative to replacing multiple active files in place.

## 5. Crash-point acceptance matrix

| Interruption point | Required behaviour |
| --- | --- |
| Before staging | Old release boots |
| Mid-stage/backup, before verified pending marker | Old release untouched; stale prep recognised safely |
| After pending, before first replacement | Old release restored/verified idempotently |
| After any individual runtime file replacement | Old complete release restored, or recovery mode if proof unavailable |
| Between runtime files and updater boot-hook replacement | No half-version launch; old release restored safely |
| After final file but before commit marker | Pending still means restore the old version |
| After committed marker, before cleanup | Validate and use the complete new release |
| Backup lost or hash mismatch | No partial rollback; keep evidence and enter safe recovery |
| Journal malformed or path traversal | Reject without executing, deleting or rewriting arbitrary paths |
| Read-only or unexpectedly removed SD card | Do not attempt writes or launch mismatched runtime; stock-safe behaviour must be tested |
| Repeated reboot while rollback is in progress | Same final old generation; never consume backups mid-rollback |

## 6. Executable host model

`tests/models/model_update_recovery.py` provides a **disposable host filesystem model** with versioned `pending` and `committed` journal states, SHA-256 checks, full-backup preflight and idempotent restore.

`tests/models/test_update_powerloss_model.py` covers **13 test methods**, including a loop over every mutation boundary of a three-file update. Run from repository root:

```sh
python3 -m unittest discover -s tests/models -p 'test_update_powerloss_model.py' -v
```

The tests assert the safety **invariants**; they do not exercise `tools/update/flip-update-install`, `tools/device/updater-playtest`, device mount behavior or the real RK3566. Keep this distinction in CI descriptions.

## 7. Implementation gates (in priority order)

1. Reconcile the newer uncommitted MSI source and the existing separate draft #44 before touching production updater code.
2. Decide on an early, trusted boot recovery entry independent of an incompletely replaced executable; demonstrate the stock boot fallback.
3. Introduce a versioned update transaction protocol with preverified immutable backups, a fully recorded file list, committed-marker validation and a read-only-safe failure path.
4. Add fault injection for normal errors, `SIGKILL`, interrupted backup preparation, interrupted rollback, torn manifests, invalid paths and missing storage. Confirm exact original bytes and unmodified user saves.
5. Run full `make verify` and GitHub release package smoke tests against the combined source; exercise BusyBox shell and FAT32/vfat on **disposable SD media**.
6. Run controlled power interruption only on disposable backups with separately verified stock-recovery access. Record complete boot logs and release hashes.
7. Keep releases blocked until normal GB/GBC/GBA, lid resume, Wi-Fi, charging, recovery and rollback pass real-device acceptance.

**Decision:** prepare and test the protocol now; **do not wire a boot guard or claim power-loss-safe updates yet**. A wrong fail-closed path can brick *the custom boot experience* even when NAND remains untouched, so the device fallback must be proven first.
