# FlipOS Engineering Audit

## Audit status

Started: 2026-10-12

Release baseline:

- `main`: `209403bd1471` — Beta 3 hotfix.
- cleanup baseline: `79b2755d1995` — three cleanup commits on top of Beta 3.
- audit branch: `audit/full-engineering-2026-10-12`.

The cleanup commits are intentionally **not** on `main` and are not considered
validated until the complete build/test suite runs.

The connected development MSI is currently offline, so this initial pass is a
source/history/build-graph audit only. No build, host test or physical-device
result is claimed in this document until it has actually run.

## Initial assessment

The project already has several strong engineering foundations:

- shared emulator Core Host rather than per-emulator lifecycle copies;
- explicit Save Safety generations and recovery paths;
- host unit tests plus architecture tests;
- an aggregate verification target;
- runtime-only voltage experimentation with boot recovery state;
- telemetry/performance instrumentation;
- vendored-source licensing/provenance documentation;
- update payload allowlisting and checksum validation.

The main risks are concentrated in interruption/failure behavior: update
transactions, save/recovery transaction ordering, hardware-service shutdown,
and proving that build/release verification works from a completely clean tree.

## Findings

### High — A001: Save Safety can change cartridge timeline before resume is blocked

Affected file: `src/runtime/flip_recovery.c`.

Evidence: `flip_recovery_restore_previous()` currently preserves the current
state, promotes the previous cartridge save, and only then creates the
`.restore-pending` resume-block marker.

If save promotion succeeds but marker creation fails (for example because the
recovery directory/filesystem becomes unwritable), the function returns an
error after the cartridge save has already changed. The old current savestate
can still remain eligible for resume, creating a mismatched save/state timeline.

Required fix: establish the durable resume block before mutating the cartridge
save, then cleanly unwind the marker if save promotion itself fails. Add a
failure-path regression test.

### High — A002: interrupted update can leave a mixed runtime without automatic rollback

Affected files:

- `tools/update/flip-update-install`;
- `tools/device/updater-playtest`.

Evidence: the installer appends each path to rollback `installed.list` only
after the live file has been replaced. A power loss between replacement and
journal update can therefore leave a changed file absent from the rollback
journal. The boot hook does not currently detect an interrupted update lock and
replay the rollback transaction.

This is a distribution-blocking reliability issue. It needs an explicit
transaction state machine and boot-time interrupted-update recovery before
FlipOS should claim power-loss-safe updating.

No change is made in the initial pass because the recovery design must be
covered by fault-injection tests before deployment.

### High (likely, not yet reproduced) — A003: audio shutdown can wait indefinitely on a blocked writer

Affected file: `src/audio/flip_audio.c`.

The writer thread performs blocking writes to the `aplay` pipe.
`flip_audio_close()` asks the thread to stop and joins it before closing the
pipe or terminating `aplay`. If `aplay` is alive but no longer draining the
pipe, the writer can remain inside `write()` and the join can block shutdown
indefinitely.

A bounded shutdown/failure test is required before changing this path.

### Medium — A004: `make verify` is not demonstrably clean-tree/release complete

Affected file: `Makefile`.

`verify` runs `test-release-package` before guaranteeing all release package
artifacts exist. The release test itself asserts that prebuilt binaries/modules
are already present. On a clean checkout this makes verification dependent on
previous build state instead of fully declaring its prerequisites.

Required fix: express release build inputs as dependencies and make the release
packaging test self-contained.

### Medium — A005: updater can associate a tag with a different release asset

Affected file: `tools/update/flip-update-fetch`.

The fetcher extracts the first matching asset URL from the complete releases
JSON but independently extracts the first `tag_name`. If the newest release
does not contain the requested asset but an older release does, `ASSET_URL`
and `TAG` can refer to different releases.

This can produce incorrect "latest" decisions and misleading diagnostics.
Parsing must preserve release/asset association.

### Medium — A006: rename-based persistence does not explicitly persist parent-directory metadata

Affected areas include save, state, settings, session and recovery metadata.

Most writers correctly flush and `fsync()` the temporary file before
`rename()`, but do not explicitly sync the containing directory afterward.
After sudden power loss, rename durability depends on the mounted filesystem's
semantics. This matters especially for removable FAT storage.

Before changing it, confirm what durability primitives the Miyoo Flip's actual
SD filesystem/kernel support reliably and measure the latency cost.

### Medium — A007: update integrity is not independent authenticity

The staged payload has strong internal SHA-256 consistency checks and is
downloaded over HTTPS from GitHub, but `SHA256SUMS` is inside the same release
archive. A compromised release asset can therefore replace both payload and
checksums.

For wider distribution, consider an independently signed release manifest or
pinned public-key verification that the target image can support without a
large dependency.

### Medium — A008: no repository CI/static-analysis gate

There is a substantial host test suite, but no top-level project CI workflow
currently enforces it for every change. There is also no dedicated sanitizer or
static-analysis target.

Add CI only after the clean-tree `verify` path is made deterministic.

## Performance and power status

No performance improvement is claimed by this audit yet.

The repository already includes frame/per-stage timing, audio queue diagnostics,
CPU/GPU/battery/temperature telemetry and performance capture tooling. Those
should be used to establish a repeatable baseline before changing hot paths.

Likewise, no voltage, frequency, regulator, bootloader, device-tree or critical
power setting will be changed as part of general refactoring.

## Prioritized backlog

### P0 — prove the baseline

1. Bring the development MSI back online.
2. Build the audit branch from a clean build directory.
3. Run `make verify`.
4. Record all compiler warnings/failures rather than suppressing them.
5. Deploy only after host verification passes, then run a short physical
   regression covering frontend, game launch, GameSwitcher, lid sleep/wake,
   audio/volume, saves and update screen behavior.

### P1 — data/recovery reliability

1. Fix A001 and add failure-path regression coverage.
2. Design and test interrupted-update recovery for A002.
3. Add full/update-storage fault injection: failed writes, failed rename,
   read-only transition, no-space condition and interrupted transaction.

### P2 — build/release quality

1. Fix A004 so clean-tree verification is self-contained.
2. Add CI for host/architecture verification.
3. Add optional ASan/UBSan host-test targets.
4. Add a compiler/static-analysis pass and triage every warning.

### P3 — runtime robustness

1. Reproduce and bound A003 audio shutdown.
2. Fix A005 release parsing with a test containing multiple releases.
3. Audit process execution, PID ownership and timeout handling.
4. Audit all atomic-write sites for filesystem-specific durability.

### P4 — measured optimization

1. Establish boot, launch, GameSwitcher, sleep/wake, input/audio and idle-power
   baselines.
2. Profile first; change only measured bottlenecks.
3. Record before/after numbers with each optimization.

## Change discipline

Every audit change should state:

- the defect/opportunity;
- evidence;
- risk;
- tests added or updated;
- tests actually run;
- hardware confirmation status where relevant.

No result should be marked verified merely because code was edited or compiled.
