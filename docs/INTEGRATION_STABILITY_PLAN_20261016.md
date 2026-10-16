# FlipOS offsite stability integration candidate — 16 October 2026

**Status: DRAFT; NOT hardware-accepted; NOT merged or installed.**

This branch combines the exact reviewed source blobs from six open, unmerged draft PRs
against the published `main` baseline `6d6eb28e93e1eee24cb9e0b23b64d3641c6bee19`. The branch's parent
is PR #48's reviewed release-evidence gate. Integration is intentionally **not**
a claim that the latest uncommitted MSI source or physical Miyoo Flip has been
updated. It is a staging branch for host compilation and conflict discovery.

## Included changes (source origins)

| PR | Included code and tests | Remaining limits |
| --- | --- | --- |
| #43 | `flip_recovery.c`, recovery-transaction test | Resume marker moves ahead of mutations; not atomic across multiple files or SD power cuts |
| #44 | `flip-update-install`, installer fault-injection test | Full rollback ledger before first replace; old rollback snapshot is still destroyed when a new backup starts; no boot-time interruption recovery |
| #45 | Updater power-loss protocol design and 13-test filesystem **model** | Python simulation only, no production boot recovery component |
| #46 | Consent-only **off-device** metrics exporter and 16 tests | No on-device UI, uploader, telemetry server or background reporting |
| #47 | Dynamic CPU guard against non-finite FPS and four native C regression scenarios | No voltage modifications or real-device power/performance acceptance |
| #48 | Evidence validator, template, 17 tests and documentation | Hashes verify file consistency, **not** the truth of hardware-acceptance claims |

The shared GitHub workflow has one combined block for the #45, #46 and #48
host-only Python suites. `make verify` then exercises the native recovery,
updater and CPU-controller tests, architecture checks and ARM64 binaries.
The existing release-package dry run remains unchanged.

## Locally reproducible host and CI checks

```sh
python3 -m unittest discover -s tests -p 'test_release_readiness.py' -v
python3 -m unittest discover -s tests/models -p 'test_update_powerloss_model.py' -v
python3 -m unittest discover -s tests/diagnostics -p 'test_*.py' -v
make test-recovery test-update-installer test-cpu-dynamic
make verify
make test-release-package
git diff --check
```

Only the current CI's commands and targets should be trusted; `make test-release-package`
requires specific pinned legacy kernel-module inputs that CI restores and verifies
separately. Use the exact branch commit and archive hashes when capturing evidence.

## Required MSI reconciliation — do not overwrite existing work

1. Reconnect the MSI and **read-only inventory** the original
   `/home/anthony/projects/FlipOS` dirty `main` checkout and isolated
   `/home/anthony/projects/FlipOS-audit-20261016` worktree. Capture hashes,
   `git status --porcelain=v1` and the exact baseline; preserve all untracked
   Oct 15 Save Manager and protected checkpoint files. No reset, checkout,
   auto-stash, clean, pull or cherry-pick in either original directory.
2. Copy to a new, separate integration worktree based on the newest local
   development snapshot. Confirm external backups of *real game saves*
   before any later hardware operation. Never use test fixtures pointed at
   genuine save paths.
3. Reconcile PR #43 with the newer checkpoint-aware `flip_recovery.c`
   and Save Manager. Preserve the correct marker ordering; verify marker
   persistence, state matching, existing restore/undo semantics and optional RTC.
4. Review PR #44's installer carefully; implement **boot-time** incomplete
   transaction recovery only after proving a trusted boot hook and safe
   previous-generation retention. The #45 model is not an implementation.
5. Reconcile #47 with newer dynamic governor work; preserve learned floor and
   original CPU policy restore. Leave experimental voltage calibration off.
6. Keep #46 metrics export on the *developer PC* until a separate consent,
   preview, privacy and on-device UI design has been accepted.
7. Keep #48's stable-release evidence gate **fail-closed**. Passing synthetic
   tests or `make verify` is not release authorisation. The manifest must
   point to the final exact package plus human-reviewed real-device test
   evidence. Do not fabricate/retroactively infer such evidence.
8. Review and rerun every test on the reconciled MSI worktree. Resolve
   formatting, CI and system-package differences. Require explicit approval
   for controlled device tests using disposable copies of saves, never
   live progress. Test read-only/failure paths, sleep/RTC, audio, GPU,
   settings and the physical updater/stock recovery escape route.

## Merge policy

Do **not** merge this rollup or the six draft PRs into `main` yet. Merging
them separately after this rollup without a reconcile plan could duplicate
work or silently lose MSI-only edits. Treat this branch as an *offsite*
integration proof, not the release line. One reviewed reconciled source tree,
full host/build CI and **device acceptance** must precede any release merge.

**Release scope:** native GB/GBC/GBA only; no new emulator, RetroArch,
NAND/MTD changes or experimental undervolting.
