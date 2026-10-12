# FlipOS v0.1.0-beta.4

**Beta 4  Autonomous CPU optimization, verified tuned profiles, and live optimizer UX.**

Beta 4 builds on the recovery work in Beta 3 and moves the RK3566 CPU
optimizer from an engineering workflow into a self-contained FlipOS feature.
The device now performs its own voltage search, validates the resulting profile,
and only allows tuned voltages during normal gameplay after the matching profile
has passed Stability Test validation.

## CPU Optimizer

### FlipOS now performs the voltage search itself

The optimizer walks each tunable RK3566 CPU OPP on-device rather than relying on
manual ADB voltage changes.

The first live candidate for each OPP is deliberately just one RK860 hardware
step (12.5 mV) below the known-stable point. After that initial proof, the
optimizer can descend in 25 mV steps until it finds an unstable boundary and
then refine that bracket at 12.5 mV resolution.

A 25 mV production guardband is added above the lowest proven stable point
before the final profile is published.

Low-frequency OPPs remain stock-preserved.

### Crash-safe autonomous recovery

Calibration still reserves CPU3 as the recovery lane while three cores are
stressed. The hardware watchdog, realtime deadman, PMU/TSADC recovery path,
forward-progress monitoring, and durable trial state remain authoritative if a
candidate becomes unstable.

A failed candidate can therefore reboot, reconcile the unfinished trial, mark
that voltage unstable, and continue/recover without a manual voltage-table
repair.

## Tuned profiles must now pass validation before gameplay

A calibrated profile is no longer treated as ready for normal use immediately.

The UI now distinguishes:

- **CALIBRATED**  optimization completed, Stability Test still required;
- **VERIFIED**  the matching profile passed Stability Test;
- **DISABLED**  the tuned profile is quarantined and stock settings remain in use.

The normal gameplay runtime path independently checks the validation result,
device identity, and calibration epoch before applying tuned voltages.

The separate validation path is still allowed to apply an unverified profile
because it must test that profile before it can become VERIFIED.

## Live optimizer progress

The CPU Optimizer progress screen is now dynamic rather than a static
"performance levels complete" splash.

It shows:

- overall calibration percentage derived from the remaining voltage-search
  uncertainty rather than a fake elapsed-time percentage;
- five segmented tunable CPU performance levels;
- the current frequency and candidate voltage;
- the active stage, including prepare, apply, recovery setup, integrity test,
  stress test, restore, and settlement;
- a live stress-window countdown;
- completed voltage-trial count;
- an adaptive estimated time remaining.

Transient progress telemetry is kept in /tmp (tmpfs/RAM), so the display does
not introduce unnecessary SD-card writes while a deliberately marginal voltage
is active.

The progress UI also runs at low priority and stays off the dedicated recovery
CPU during destructive calibration.

## Redesigned Stability Test screen

The Stability Test now presents the validation workload more clearly.

The screen includes:

- a large real stress-time countdown;
- a five-level CPU validation timeline;
- the current MHz and tuned voltage;
- four live per-core utilisation meters sampled from /proc/stat;
- current load-window progress;
- CPU temperature and observed peak temperature;
- explicit recovery-protection status.

Calibration deliberately stresses three cores, but the Stability Test stresses
all four cores. The live C0-C3 meters therefore give visible confirmation that
the verified profile is being tested under full CPU load.

The Stability Test repeatedly parks and re-enters each tuned OPP between load
windows so frequency/voltage transition behaviour is exercised as well as
steady-state load.

## Real-device validation

On the Miyoo Flip used for development, Beta 4 completed a full autonomous
calibration and then passed a 10-minute Stability Test across all five tunable
performance levels.

The validation used 120 seconds per tunable OPP and reached a recorded peak SoC
temperature of 68.75 C.

The resulting voltages are **device-specific and are not shipped as a universal
RK3566 voltage table**. Every Flip calibrates its own CPU.

Automatic recovery from every possible silicon failure mode cannot be
guaranteed, so FlipOS remains Beta software. Keep normal backups of important
saves and report any optimizer, recovery, stability, or update problems.

## Downloads

- **FlipOS-Fresh_Install.zip**  for a new SD-card installation.
- **FlipOS-Update.tar.gz**  for updating an existing FlipOS installation.

A legacy-named **FlipOSUpdate.tar.gz** copy is also included for compatibility
with older Beta updater builds.

## Updating

Connect to Wi-Fi, open the FlipOS update screen, check GitHub for updates, and
install Beta 4.

The update package is validated and staged before installation. Do not remove
power or the SD card while an update is being installed.

## Please report issues

If you see an optimizer lockup, failed recovery, Stability Test problem, update
problem, crash, failed resume, game issue, Wi-Fi problem, battery/power issue,
or anything else that looks wrong, please report it:

https://github.com/AnthonyWCurtis/FlipOS/issues
