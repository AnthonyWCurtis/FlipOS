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
