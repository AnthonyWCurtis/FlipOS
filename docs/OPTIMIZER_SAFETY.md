# FlipOS Optimizer Safety Contract

## Scope

The CPU optimizer must never make an unvalidated voltage profile persistent
across a reboot. The initial calibration implementation is restricted to
runtime-only kernel-supported voltage/OPP changes.

## Durable trial state

flip-optimizer-guard stores three independent pieces of state under
/mnt/SDCARD/flipos-dev/state/optimizer:

- trial-active — a candidate has been armed and has not been confirmed;
- trial-last-event — audit record for arm, confirm or rollback;
- stock-only — durable latch that disables automatic optimizer retry.

The active marker is written and fsynced before any future experimental voltage
change is allowed to occur.

## Boot behavior

The playtest boot wrapper runs flip-optimizer-guard boot-check before the
playtest logger and before flip-frontend.

If no active trial exists, boot continues normally.

If an active trial remains, the previous run is treated as unconfirmed. The
guard records rollback, removes the active marker and sets stock-only. Because
initial voltage experiments are runtime-only, the reboot has already returned
the kernel to its stock device-tree OPP table before this userspace guard
executes.

If the trial marker is corrupt, the result is also stock-only. Corruption must
never be interpreted as successful calibration.

## Confirmation

A stable calibration run must explicitly confirm the matching trial ID before
reboot. Confirmation removes the active marker and records a confirmed event. A
mismatched trial ID cannot confirm another trial.

After rollback, a new trial cannot be armed while stock-only is present. An
explicit operator/calibration action must clear the latch before trying again.

## Persistent-voltage prohibition

Do not modify a boot DTB, bootloader-selected OPP table, or any other persistent
pre-kernel voltage source under this safety model. A userspace boot guard is too
late to recover a voltage table that prevents the kernel from booting.

Persistent boot-time undervolting requires a separately proven fallback path
that operates before the experimental kernel voltage table is needed.

## Hardware acceptance

On 2026-10-06 an unconfirmed fake trial survived a reboot and was automatically
rolled back before the frontend started. The active marker was cleared,
stock_only=1 was set, the rollback event was recorded, and the stock 408-1992
MHz ondemand CPU policy was live.

A second fake trial was explicitly confirmed before reboot; the subsequent boot
remained safe with no rollback and no stock-only latch. No CPU voltage was
written during either proof.

## Calibration-state integration

The per-device calibration state persists the exact pending OPP, candidate
microvolts and deterministic trial ID. A future candidate runner must save that
state and arm the matching optimizer-guard trial before calling any voltage
backend.

If the device reboots before the candidate is explicitly recorded as stable,
the boot guard rolls the trial back and the calibration framework can map the
matching rollback event to the pending candidate as unstable. This preserves
both fail-safe boot behavior and the search boundary.

The calibration probe itself is read-only with respect to CPU voltage.

The calibration transaction layer persists the pending candidate before arming
the guard. A future voltage backend is permitted to run only when
ready_for_apply confirms that:

- calibration state is valid and has a pending candidate;
- optimizer-guard has an active trial with the exact same trial ID;
- the stock-only latch is not set.

At boot, guard rollback processing runs first and calibration reconciliation
runs immediately afterward, both before telemetry or the frontend. A matching
rolled-back trial is therefore converted into an unstable calibration bound
automatically. A matching confirmed event can likewise recover a confirmation
if power was lost after guard confirmation but before calibration state was
committed.

Hardware dry-run acceptance used the real first candidate at 1992 MHz /
1087.5 mV. The transaction reached ready_for_apply=1 while the kernel OPP
remained at the stock 1100 mV. Rebooting without confirmation produced an
automatic guard rollback and calibration reconcile. The artificial result was
then discarded and the clean plan reinitialized. No voltage was written.

## Hardware watchdog limitation

The RK3566 DesignWare watchdog at /dev/watchdog0 was tested on real hardware
before live undervolting. With an eight-second timeout, stopping the userspace
feeder caused the hardware watchdog to fire and ADB/USB to disappear. However,
the Miyoo Flip did not return to a normal autonomous boot/USB state afterward.
A manual power-cycle was required.

The durable optimizer trial survived that event. On the following manual boot,
optimizer-guard ran before telemetry/frontend, recorded the unconfirmed
watchdog proof trial as rolled back, set stock-only, and the kernel returned to
its stock device-tree CPU OPP table and 408-1992 MHz ondemand policy.

Therefore the DesignWare watchdog is useful as a hang detector but is not
accepted as an unattended full-board recovery mechanism on this Miyoo hardware.
Initial real voltage candidates remain runtime-only and non-persistent, so a
manual power-cycle always restores the stock kernel OPP table. A hard hang may
require that manual power-cycle.

No persistent undervolt profile may be enabled on the strength of this
watchdog test.

## Self-healing supervisor architecture — 2026-10-07

The first live undervolt sweep exposed a second failure mode that the original
guard did not cover: after a hard crash and manual power-cycle, Linux mounted
the FAT SD partition read-only (`errors=remount-ro`). The original boot wrapper
attempted SD logging and durable guard settlement before making its safety
decision, so a read-only filesystem could prevent recovery bookkeeping from
running at all.

`flip-optimizer-supervisor` now separates the safety decision from durable
bookkeeping. Its boot inspection is read-only. Before any mkdir, chmod, log
redirect or optimizer-state write, the playtest boot wrapper runs
`boot-enforce`. Any active, corrupt or unreadable trial forces a RAM-only
`/tmp/flipos-optimizer-stock-safe` latch. The live candidate runner refuses to
apply a voltage candidate while this latch exists. If FAT is read-only, logs
move to `/tmp` and durable guard/calibration settlement is deferred until a
later writable boot. Safe behavior therefore no longer depends on the SD card
accepting a write.

### Real-time deadman

A separate `flip-optimizer-deadman` executable is loaded and armed before the
CPU enters a candidate voltage. The candidate waits for an explicit readiness
handshake after the deadman has:

- touched its working memory;
- called `mlockall(MCL_CURRENT | MCL_FUTURE)`;
- raised itself to maximum `SCHED_FIFO` priority; and
- established a monotonic absolute deadline.

No SD I/O is required after that handshake. On timeout the deadman performs a
direct `SYS_reboot` using `LINUX_REBOOT_CMD_RESTART2` with the command
`watchdog`. It intentionally does not call `sync()`, execute a shell, or spawn
another process. The Rockchip syscon reboot-mode notifier can therefore tag the
restart as `BOOT_WATCHDOG` in PMUGRF before the platform restart occurs.

The candidate keeps the deadman armed while experimental voltage is active and
through stock-table/policy restoration. It disarms the process only after the
dangerous phase is over. Failure to disarm is itself treated as unsafe and
forces an emergency restart.

### Controlled versus uncontrolled failure

A bounded test failure is no longer automatically converted into a reboot.

If the candidate voltage was genuinely entered, the test reports failure, and
FlipOS successfully restores the parking frequency, stock OPP table, module
state and original CPU policy, the guard records a `failed` event and the
calibration commits that candidate as unstable. No reboot is needed.

If the run aborts before candidate voltage is entered, the pending trial is
cancelled and does not count as instability evidence.

If execution is lost while candidate voltage is active, no completion event is
written. The still-armed durable trial plus the next boot's reset cause become
the evidence used by boot recovery.

### RK3566 reset-cause classification

The stock Rockchip 5.10 kernel already exposes `/sys/kernel/boot_mode` through
`syscon-reboot-mode`. The RK3568/RK3566 device tree defines normal, panic and
watchdog reboot magic values in the PMUGRF syscon at offset `0x200`.

On this physical Flip, the manual power-cycle following the hard calibration
crash produced:

- `/sys/kernel/boot_mode`: `coldboot`
- PMUGRF reboot-mode register after boot: `0x00000000`

The supervisor now reads this interface without modifying it and classifies
`coldboot`, `normal`, `panic`, `watchdog`, unknown and other modes. This
allows an unfinished optimizer trial to be paired with platform reset evidence
rather than treating every incomplete run as the same event.

The kernel also has ramoops/pstore enabled with reserved persistent RAM. Its
pstore directory was empty after the manual power-cycle, but it remains a
candidate secondary source for later panic/oops evidence.

Hardware acceptance for the deadman restart tag and read-only-SD recovery
path completed on 2026-10-07. The RAM-locked SCHED_FIFO deadman autonomously
rebooted the physical Flip, ADB returned without manual intervention and the
next kernel reported `boot_mode=watchdog`. A fake unconfirmed optimizer trial
was rolled back before normal startup, `stock_only=1` was asserted and the
runtime stock-safe latch remained active.

### PMUGRF recovery token

Repeated forced resets demonstrated that FAT metadata can be damaged even when
the optimizer uses temp-file + fsync + rename + directory-fsync semantics.
Observed failures included a zero-byte `trial-active`, FAT
`fat_free_clusters: deleting FAT entry beyond EOF`, automatic read-only
remount and a dirty filesystem requiring `fsck.vfat`.

FlipOS therefore no longer relies on FAT for crash-time CPU candidate identity.
RK3566/RK3568 PMU_GRF_OS_REG1 at physical address `0xfdc20204` is used as a
32-bit warm-reset recovery token. The token encodes the known CPU OPP, the
12.5 mV candidate voltage step and deterministic trial number with
magic/version/check bits. FAT remains the durable human-readable calibration
journal, but PMUGRF is the crash-survival source of truth for an armed CPU
candidate.

Hardware acceptance proved:

- OS_REG1 was initially zero, accepted a temporary test value and retained it
  exactly across a normal warm reboot;
- the temporary value was then cleared back to zero;
- arming CPU-format trial `cpu-1416000-912500-63` produced a non-zero PMU
  token and a watchdog reboot recovered that exact trial before FAT settlement;
- durable rollback recorded the exact trial ID, asserted stock-only quarantine
  and cleared OS_REG1 only after settlement completed;
- with trial `cpu-1608000-987500-63` armed, the SD was deliberately remounted
  read-only; the supervisor still recovered the exact trial from PMUGRF and
  asserted the RAM quarantine;
- guard settlement correctly deferred while FAT was read-only and the PMU token
  remained intact;
- after remounting read-write, durable rollback completed for the exact trial
  and only then cleared OS_REG1.

The forced-reset tests also showed that concurrent frontend/playtest logging can
leave FAT dirty even though optimizer state itself is not written during the
dangerous window. Before the next live undervolt sweep, FlipOS must explicitly
quiesce SD-writing telemetry/frontend activity during synthetic candidate
testing and restore it only after stock voltage is confirmed.