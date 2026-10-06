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
watchdog test.\n