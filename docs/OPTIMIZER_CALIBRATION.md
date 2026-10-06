# FlipOS Per-Device CPU Calibration Framework

## Status

Framework accepted on real Miyoo Flip hardware on 2026-10-06.

This layer is intentionally read-only with respect to CPU voltage. It discovers
hardware limits and builds durable calibration state, but it cannot apply a
voltage candidate yet.

## Device identity

Calibration is keyed to the RK3566 CPU serial reported by /proc/cpuinfo, not
the USB enumeration path. On the current test unit:

- device ID: b9f4ffca2f298ad2
- model: MIYOO RK3566 355 V10 Board

## Live hardware discovery

flip-optimizer-calibration probe reads:

- CPU serial from /proc/cpuinfo;
- board model from /proc/device-tree/model;
- vdd_cpu regulator minimum from /sys/class/regulator;
- available CPU OPP frequencies and selected stock targets from
  /sys/kernel/debug/opp/cpu0.

The RK860 electrical regulator floor is 712.5 mV, but FlipOS does not use that
as the silicon floor. The RK3566 VDD_CPU specified operating minimum is 0.81 V;
with 12.5 mV regulator steps the optimizer rounds that upward to an effective
812.5 mV calibration floor. A candidate below 812.5 mV is never generated.

Current discovered plan:

| CPU OPP | Stock voltage | Regulator floor | First candidate |
| --- | ---: | ---: | ---: |
| 408 MHz | 850.0 mV | 812.5 mV | 837.5 mV |
| 600 MHz | 850.0 mV | 812.5 mV | 837.5 mV |
| 816 MHz | 850.0 mV | 812.5 mV | 837.5 mV |
| 1104 MHz | 850.0 mV | 812.5 mV | 837.5 mV |
| 1416 MHz | 925.0 mV | 812.5 mV | 912.5 mV |
| 1608 MHz | 1000.0 mV | 812.5 mV | 987.5 mV |
| 1800 MHz | 1050.0 mV | 812.5 mV | 1037.5 mV |
| 1992 MHz | 1100.0 mV | 812.5 mV | 1087.5 mV |

## Search policy

The framework:

1. tests the highest incomplete OPP first;
2. lowers one RK860 step at a time: 12.5 mV;
3. persists the exact candidate and deterministic trial ID;
4. arms the matching optimizer boot guard;
5. exposes ready_for_apply=1 only when both durable states match and the
   stock-only latch is clear;
6. records stable candidates as the new lower stable bound;
7. treats a matching optimizer-guard rollback as an unstable candidate;
8. finalizes an OPP with a 25 mV guardband above the lowest proven-stable
   voltage, capped at the stock voltage;
9. enforces monotonic final voltages before producing an optimizer result.

A reboot during a candidate test therefore cannot lose which voltage was being
tested.

## Persistent state

State is stored at:

/mnt/SDCARD/flipos-dev/state/optimizer/cpu-calibration

The initial plan was written and reloaded successfully on hardware.

A full transaction dry-run was then performed for the first 1992 MHz candidate:

- candidate: 1087.5 mV;
- pending state persisted with trial ID cpu-1992000-1087500-1;
- matching optimizer guard armed;
- ready_for_apply became 1;
- live 1992 MHz OPP target remained unchanged at the stock 1100 mV;
- no voltage was written.

The Flip was rebooted without confirming that dry-run candidate. On the next
boot, optimizer-guard rolled the trial back before the frontend, and the
calibration reconciler automatically mapped the same trial ID to the pending
candidate. The stock-only latch prevented another candidate from being armed.

Because no voltage had actually been applied, that intentionally artificial
result was discarded. The stock-only latch was explicitly cleared and the
per-device plan was reinitialized cleanly with:

- pending: 0;
- completed OPPs: 0;
- optimizer guard active: 0;
- stock-only latch: 0;
- CPU policy: stock 408-1992 MHz ondemand;
- live 1992 MHz OPP target: stock 1100 mV.

## Safety boundary

No command in flip-optimizer-calibration can write CPU voltage. Its exposed
commands manage discovery and calibration/guard state only:

- probe
- init
- status
- prepare
- ready
- reconcile
- confirm-stable
- clear-stock-only

prepare persists a candidate and arms the guard but explicitly reports
voltage_written=0. ready is the mandatory gate a future runtime voltage backend
must pass before it is allowed to apply a candidate.

The boot wrapper runs optimizer-guard first, then calibration reconcile, then
the telemetry logger and frontend. This ordering is protected by an architecture
test.

The next step is to identify and prove a kernel-owned runtime voltage/OPP update
mechanism. Raw RK860 I2C writes and persistent boot-time DTB/OPP changes remain
prohibited.
