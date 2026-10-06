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

Current discovered plan:

| CPU OPP | Stock voltage | Regulator floor | First candidate |
| --- | ---: | ---: | ---: |
| 408 MHz | 850.0 mV | 712.5 mV | 837.5 mV |
| 600 MHz | 850.0 mV | 712.5 mV | 837.5 mV |
| 816 MHz | 850.0 mV | 712.5 mV | 837.5 mV |
| 1104 MHz | 850.0 mV | 712.5 mV | 837.5 mV |
| 1416 MHz | 925.0 mV | 712.5 mV | 912.5 mV |
| 1608 MHz | 1000.0 mV | 712.5 mV | 987.5 mV |
| 1800 MHz | 1050.0 mV | 712.5 mV | 1037.5 mV |
| 1992 MHz | 1100.0 mV | 712.5 mV | 1087.5 mV |

## Search policy

The framework:

1. tests the highest incomplete OPP first;
2. lowers one RK860 step at a time: 12.5 mV;
3. persists the exact candidate and deterministic trial ID before a future
   voltage backend is allowed to apply it;
4. records stable candidates as the new lower stable bound;
5. treats a matching optimizer-guard rollback as an unstable candidate;
6. finalizes an OPP with a 25 mV guardband;
7. enforces monotonic final voltages before producing an optimizer result.

A reboot during a candidate test therefore cannot lose which voltage was being
tested.

## Persistent state

State is stored at:

/mnt/SDCARD/flipos-dev/state/optimizer/cpu-calibration

The initial plan was written and reloaded successfully on hardware, then
survived a real reboot with:

- pending: 0
- completed OPPs: 0
- optimizer guard active: 0
- stock-only latch: 0
- CPU policy: stock 408-1992 MHz ondemand

## Safety boundary

No command in flip-optimizer-calibration can write CPU voltage. Its exposed
commands are currently only:

- probe
- init
- status

The next step is to identify and prove a kernel-owned runtime voltage/OPP update
mechanism. Raw RK860 I2C writes and persistent boot-time DTB/OPP changes remain
prohibited.
