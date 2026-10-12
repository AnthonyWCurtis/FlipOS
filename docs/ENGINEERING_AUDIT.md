# FlipOS Engineering Audit

## Audit status

Started: 2026-10-12

Release baseline:

- `main`: `209403bd1471` — Beta 3 hotfix.
- cleanup baseline: `79b2755d1995` — three cleanup commits on top of Beta 3.
- audit branch: `audit/full-engineering-2026-10-12`.

The cleanup/audit commits are intentionally **not** on `main`.

Baseline verification on 2026-10-12 used a separate clean worktree and the
prepared matching Rockchip 5.10.160 kernel tree. After fixing one stale Core
Host test stub left by the hotkey refactor, a true `rm -rf build` rebuild and
`make KERNEL_DIR=... verify` completed with `VERIFY: PASS`. This includes host
unit tests, architecture tests, release packaging, AArch64 userspace builds and
the four release kernel-module targets.

The live device was queried read-only and reports Linux 5.10.160 on
`MIYOO RK3566 355 V10 Board`; no audit runtime was deployed to hardware during
this baseline step.

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

Status: fixed on the audit branch. The durable resume block is now created
before cartridge-save promotion. If a later recovery step fails, the marker is
left in place deliberately so resume fails closed rather than reusing a state
from the wrong save timeline. A regression test forces marker creation to fail
and verifies that the active save/state pair remains unchanged.

### High — A002: interrupted update can leave a mixed runtime without automatic rollback

Affected files:

- `tools/update/flip-update-install`;
- `tools/device/updater-playtest`.

Evidence: the installer appends each path to rollback `installed.list` only
after the live file has been replaced. A power loss between replacement and
journal update can therefore leave a changed file absent from the rollback
journal. The boot hook does not currently detect an interrupted update lock and
replay the rollback transaction.

Status: fixed on the audit branch, but not yet deployed to hardware. The
installer now uses a versioned transaction lock, a write-ahead rollback journal
and durable `prepared`/`committed` markers. Every path is journaled and synced
before its atomic rename can make new bytes live, including the separate SD boot
hook. A surviving transaction lock is recovered before any FlipOS runtime
component is executed; recovery failure deliberately falls back to the stock
Miyoo launcher rather than booting a mixed runtime.

Host fault-injection tests now simulate process death after rollback
preparation, before the first rename, after the first live replacement, around
the boot-hook replacement, immediately before commit and immediately after the
durable commit marker. Pre-commit cases restore the previous runtime; the
post-commit case keeps the new runtime and only finalizes the stale lock.

Bootstrap limitation: Beta 3's already-installed boot hook does not contain the
new early recovery call. The first deployment of this recovery-capable updater
must therefore be done as a controlled development deployment with a known
recovery path. Once that baseline is installed, subsequent update transactions
use the boot-time recovery protocol.

### High — A003: audio shutdown can wait indefinitely on a blocked writer

Affected files:

- `src/audio/flip_audio.c`;
- `src/audio/flip_audio_transport.c`.

Evidence: the original writer used a blocking pipe to `aplay`, while
`flip_audio_close()` joined the writer before closing that pipe. If the child
stopped draining PCM, the writer could remain inside `write()` and the join
had no deadline. The later blocking `waitpid()` on `aplay` was also
unbounded.

Status: fixed on the audit branch, not yet hardware-accepted. The PCM pipe is
now explicitly non-blocking and each complete transport write has a 250 ms
monotonic deadline. A stalled transport is disabled rather than allowed to
block shutdown. Child shutdown now has bounded clean-exit, SIGTERM and SIGKILL
windows and never uses an unbounded `waitpid()`.

A host regression test fills a non-blocking pipe until it cannot accept more
data and verifies that the transport returns `ETIMEDOUT` within a bounded
window. It also creates a child that ignores SIGTERM and verifies that the
shutdown helper escalates to SIGKILL and reaps it within the deadline. This
confirms the failure mechanics and the bounded recovery path on Linux; actual
Miyoo Flip audio/suspend acceptance is still required before release.

### Medium — A004: `make verify` is not demonstrably clean-tree/release complete

Affected file: `Makefile`.

`verify` runs `test-release-package` before guaranteeing all release package
artifacts exist. The release test itself asserts that prebuilt binaries/modules
are already present. On a clean checkout this makes verification dependent on
previous build state instead of fully declaring its prerequisites.

Status: fixed on the audit branch. Release-build artifacts are now explicit
prerequisites of the release-package test, and `make clean` removes the entire
generated `build/` tree instead of maintaining a fragile hand-written file
list. A fully clean rebuild with the matching kernel tree completed with
`VERIFY: PASS`.

### Medium — A005: updater can associate a tag with a different release asset

Affected file: `tools/update/flip-update-fetch`.

The fetcher extracts the first matching asset URL from the complete releases
JSON but independently extracts the first `tag_name`. If the newest release
does not contain the requested asset but an older release does, `ASSET_URL`
and `TAG` can refer to different releases.

Status: fixed on the audit branch. Release parsing now walks fields in release
order and keeps the active release tag associated with its assets. The updater
test now includes a newer release without the requested asset followed by an
older release that contains it, and verifies that the selected asset keeps the
older release's matching tag.

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

### Medium — A009: external kernel-module verification lacks symbol metadata

The matching prepared Rockchip 5.10.160 tree builds the FlipOS modules, but
`modpost` reports that the kernel `Module.symvers`/vmlinux symbol information is
missing and therefore skips unresolved-symbol checking. Both the prepared tree
and the live device report `CONFIG_MODVERSIONS` disabled, so this is not a
module-version CRC problem, but the build still loses a useful compatibility
check.

Before release automation is considered complete, preserve or reproduce a
fully built matching kernel tree (or equivalent symbol metadata) so module
undefined-symbol checks are meaningful. Do not paper over this warning.

The remaining compiler warnings observed during the clean verification are in
vendored Gambatte/mGBA sources or linker/LTO diagnostics, not original FlipOS C
sources. They should be tracked separately and not "fixed" by style-editing
vendor code.

### High — A010: automatic calibration can strand the external CPU regulator at a failed voltage

Affected files:

- `src/hwtest/flip_optimizer_calibration_tool.c`;
- `src/hwtest/flip_optimizer_candidate_tool.c`;
- `src/runtime/flip_optimizer_hw_watchdog.c`;
- `tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c`;
- `src/frontend/flip_frontend.c`.

Device evidence: several deliberately unstable calibration candidates stop at
`LIVE_BEGIN` and leave the handheld with a green LED and black display. The
candidate had already logged an 11-second hardware watchdog and a 50-second
userspace deadman, yet recovery did not occur within either deadline. Failures
were recorded at 1992 MHz / 850000 uV, 1800 MHz / 825000 uV, 1608 MHz /
812500 uV and 1800 MHz / 837500 uV. On a later manual power cycle, the durable
guard correctly identifies the unconfirmed trial and rolls it back.

Controlled hardware testing narrowed the cause. A raw watchdog-expiry probe at
stock CPU voltage successfully rebooted the Flip automatically after heartbeats
were stopped. Live CRU readback also showed watchdog reset was already enabled
as a PMU reset source on this board. The earlier bit-4 correction therefore
remains useful defensive programming and readback validation, but is not the
root cause of the black-screen failure.

The external RK860 is part of the failure, but inspection of the live device
tree and Rockchip reset documentation exposed the missing recovery link. The
live RK860 node is powered from `vcc3v8_sys`. The RK3566 GPIO0_A2 pin is the
PMIC_SLEEP / TSADC_SHUT_M1 alternate-function pin, while the live RK817 node
contains the matching PMIC reset states. Live RK817 SYS_CFG3 was already
configured for active-low SLEEP-pin restart. The previous FlipOS recovery
module only selected the TSADC device's `otpout` state, which the live DT maps
to GPIO0_A1. It never muxed GPIO0_A2 to TSADC_SHUT_M1, so a watchdog reset could
restart the SoC without forcing an RK817/VCC_SYS restart. That explains why a
stock-voltage watchdog reboot worked while a deeply undervolted RK860 could
remain stranded.

The Rockchip TSADC TRM adds an important ordering constraint: when the TSHUT
GPIO enable bit is clear, the output is held low. Directly muxing GPIO0_A2 to
TSADC_SHUT_M1 before enabling the route could therefore cause an immediate
active-low RK817 restart.

Status: a new recovery chain is implemented and fully host-verified on the
audit branch, but is not yet hardware-accepted. Arming now makes the RK817
SLEEP action inert, enables/verifies the TSADC GPIO route, checks low-active
polarity and no pending TSHUT, muxes GPIO0_A2 to TSADC_SHUT_M1 with PMUGRF
write-mask semantics, then enables RK817 active-low PMIC restart. CRU watchdog
extension/PMU routing remains enabled. Teardown reverses the path and restores
the exact original mux, RK817 bits, TSADC route and CRU bits. The module cannot
be unloaded while armed.

A second correctness fix enforces fresh-run isolation: the calibration history
file remains available for engineering diagnostics, but a new optimizer run no
longer imports prior unstable voltages into its boundary state. Every run starts
from the live stock OPP table and produces its own evidence.

The next-lower-stock voltage floor remains only as a temporary safety gate
until this new PMIC-restart path passes a stock-voltage watchdog-expiry hardware
probe. No deep undervolt recovery test will be attempted before that acceptance
step. The persistent guard and boot rollback remain independent safety layers.

## Performance and power status

No performance improvement is claimed by this audit yet.

The repository already includes frame/per-stage timing, audio queue diagnostics,
CPU/GPU/battery/temperature telemetry and performance capture tooling. Those
should be used to establish a repeatable baseline before changing hot paths.

Likewise, no voltage, frequency, regulator, bootloader, device-tree or critical
power setting will be changed as part of general refactoring.

## Prioritized backlog

### P0 — prove the baseline

Completed: development MSI online; clean audit worktree built; full
`make KERNEL_DIR=... verify` passed; warnings were recorded rather than
suppressed. Remaining: controlled audit-build deployment and the physical
regression covering frontend, game launch, GameSwitcher, lid sleep/wake,
audio/volume, saves and update-screen behavior.

### P1 — data/recovery reliability

Completed: A001 Save Safety ordering fix and regression test; A002
write-ahead update transaction, boot-time recovery and interruption
fault-injection coverage; A010 fresh-run isolation plus host-verified
TSADC_SHUT_M1 -> RK817 PMIC-restart recovery routing. Remaining: explicit
failed-write/failed-rename, read-only-transition and no-space fault injection,
controlled updater hardware acceptance, and a stock-voltage hardware acceptance
probe of the new PMIC-restart recovery path before deeper optimizer testing.

### P2 — build/release quality

Completed: A004 clean-tree/release verification fix. Remaining: add CI for
host/architecture verification, optional ASan/UBSan host-test targets, and a
compiler/static-analysis pass with warning triage.

### P3 — runtime robustness

Completed: A003 bounded audio transport/child shutdown with host stall tests;
A005 release/asset association fix with a multi-release regression test.
Remaining: physical audio/suspend acceptance for A003, audit process/PID and
timeout handling, and audit atomic-write sites for filesystem-specific
durability.

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
