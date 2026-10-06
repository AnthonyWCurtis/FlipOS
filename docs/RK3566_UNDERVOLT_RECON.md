# RK3566 Undervolt Recon — Miyoo Flip

Date: 2026-10-06
Scope: read-only reconnaissance only. No CPU/GPU voltage or clock values were written.

## CPU frequency policy

- SoC: Rockchip RK3566, four CPU cores under one cpufreq policy.
- Driver: `cpufreq-dt`.
- Governor observed: `ondemand`.
- Available CPU OPPs: 408, 600, 816, 1104, 1416, 1608, 1800, 1992 MHz.
- Policy range: 408–1992 MHz.
- All CPUs 0-3 are affected by the same policy.

## CPU voltage rail

- Regulator: `vdd_cpu`.
- Physical regulator: Rockchip RK8600 at I2C0 address `0x40`.
- Kernel driver: `rk860-regulator`.
- Device-tree regulator range: 712.5–1387.5 mV.
- Configured ramp delay: 2300 uV/us.
- RK8600 hardware family uses a 712.5 mV base and 12.5 mV selector step for chip ID 0x8.
- Kernel boot log reports `RK860 chip ID:88`; the loaded driver and DT constraints are consistent with the 12.5 mV RK8600 variant.
- Direct userspace `i2cget` is rejected with `Device or resource busy` while the regulator driver owns address 0x40. This is desirable: raw userspace writes must not race kernel DVFS.

## CPU silicon-bin selection

Boot log:

- `cpu cpu0: leakage=39`
- `cpu cpu0: pvtm = 93610, from nvmem`
- `cpu cpu0: pvtm-volt-sel=3`
- `cpu cpu0: avs=0`

The live DT contains voltage tables L0-L3. `pvtm-volt-sel=3` selects the lowest-voltage L3 entries for this unit at the OPPs that have per-level tables.

## Effective CPU OPP voltage floors

These are the live kernel OPP `u_volt_min` values from debugfs:

| CPU OPP | Effective floor |
| --- | ---: |
| 408 MHz | 850 mV |
| 600 MHz | 850 mV |
| 816 MHz | 850 mV |
| 1104 MHz | 850 mV |
| 1416 MHz | 925 mV |
| 1608 MHz | 1000 mV |
| 1800 MHz | 1050 mV |
| 1992 MHz | 1100 mV |

The DT also allows a maximum request of 1150 mV for each CPU OPP.

## CPU DT voltage levels

For OPPs with Rockchip PVTM level tables:

| OPP | L0 | L1 | L2 | L3 |
| --- | ---: | ---: | ---: | ---: |
| 1104 MHz | 900 | 850 | 850 | 850 mV |
| 1416 MHz | 1025 | 975 | 950 | 925 mV |
| 1608 MHz | 1100 | 1050 | 1025 | 1000 mV |
| 1800 MHz | 1150 | 1100 | 1075 | 1050 mV |
| 1992 MHz | 1150 | 1150 | 1125 | 1100 mV |

408/600/816 MHz have a single 850 mV table entry.

## GPU voltage rail

- GPU regulator: `vdd_gpu` on the RK817 PMIC at I2C0 address `0x20`.
- Regulator range: 500–1350 mV.
- Live GPU OPP effective floors:
  - 200/300/400/600 MHz: 850 mV
  - 700 MHz: 875 mV
  - 800/900 MHz: 925 mV
- Boot log reports GPU `pvtm-volt-sel=3`.

GPU undervolting is not a current priority because measured emulator workloads keep the GPU near 200 MHz and ~4-5% utilization.

## Safety conclusions

1. Per-device CPU undervolting is technically plausible on this hardware.
2. The CPU rail is already dynamically controlled by the kernel; raw I2C writes should not be used during normal DVFS because they can race frequency transitions.
3. The preferred production design is to make the kernel own the calibrated frequency/voltage pairs, either by a runtime kernel-supported OPP update path or by a safely generated boot-time OPP table.
4. Calibration should be per physical device; this unit is already a favorable L3/PVTM-selected part, so available undervolt margin may be smaller than on a worse-bin unit.
5. Any calibration harness must test stress, light load/idle, emulator workloads, temperature variation, suspend/resume, and long soak. Stress-only stability is insufficient.
6. A failed calibration step must always recover at stock voltage after reboot. Never persist an unvalidated voltage before recovery is proven.
7. CPU frequency-policy tuning should be evaluated before undervolting; reducing unnecessary high-OPP residency can save power without changing voltage margins.

## Next safe engineering step

Do not undervolt yet. First build a read-only/stock-voltage characterization harness that:

- pins each existing CPU OPP through the kernel cpufreq interface;
- records the corresponding live `vdd_cpu` readback;
- measures workload/FPS/audio stability and power at stock voltage;
- confirms watchdog/reboot recovery and stock-voltage restoration;
- maps the boot DTB/boot chain so a persistent OPP-table experiment has a proven rollback path.

Only after those checks should a one-step (12.5 mV) experimental undervolt be attempted.

## Optimizer rollback proof

On 2026-10-06 FlipOS completed the rollback prerequisite without modifying any
CPU voltage.

A native flip-optimizer-guard now maintains a durable trial marker and a
stock-only safety latch under state/optimizer. The boot wrapper executes the
guard before telemetry or the frontend.

Real-hardware proof:

1. A fake trial named rollback-proof-20261006 was armed and synced to SD.
2. The Flip rebooted without confirming the trial.
3. On the next boot the guard ran before the frontend, converted the stale
   trial into last_action=rolled_back, removed the active marker and set
   stock_only=1.
4. The live CPU policy after recovery was the normal stock 408-1992 MHz
   ondemand policy.
5. The latch blocked automatic re-arming until it was explicitly cleared.
6. A second fake trial was armed, explicitly confirmed, and rebooted. That boot
   remained safe with stock_only=0 and last_action=confirmed.

Corrupt trial-marker recovery is covered by host tests and also resolves to the
stock-only path.

This proves the control-plane watchdog/rollback contract before voltage writes.
The first real undervolt experiment must remain runtime-only and non-persistent
so a reboot restores the kernel's stock device-tree OPP voltages. Persistent
boot-time DTB/OPP changes remain out of scope until a pre-kernel fallback path
is mapped and tested.
