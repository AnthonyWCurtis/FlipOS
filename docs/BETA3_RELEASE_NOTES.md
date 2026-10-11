# FlipOS v0.1.0-beta.3

**Beta 3 — CPU Optimizer recovery and stability hotfix.**

This hotfix is focused on the CPU Optimizer after real-device testing exposed
hard-lock and recovery edge cases while searching for the lowest stable RK3566
voltages.

## Fixed and hardened

### CPU Optimizer recovery path

The optimizer no longer saturates all four CPU cores during destructive voltage
testing. Three cores are stressed while CPU3 is reserved as a recovery lane.

The realtime deadman is pinned to CPU3 at SCHED_FIFO priority 99 so recovery
work has a dedicated execution path if a stressed core becomes marginal.

The optimizer now also monitors forward progress from each stress worker. A
worker that stops making progress is treated as a failed trial instead of being
allowed to block indefinitely, and worker shutdown has a bounded timeout.

### Faster fallback recovery

The software deadman fallback window has been reduced substantially. The
optimizer now uses a much shorter recovery headroom instead of allowing a
failed trial to remain stuck for many minutes.

### Safer voltage search

Initial descent now uses 25 mV steps rather than 50 mV jumps. This is slower
than the original coarse search, but reduces the chance of stepping directly
from a stable voltage into a severe hard lock.

Binary refinement still takes over after the first unstable point is known.

### Full 4-core stability validation

Calibration deliberately stresses three cores and reserves CPU3 for recovery.
The separate Stability Test now uses all four RK3566 cores so the guarded final
profile is validated under maximum CPU load rather than reusing the safer
three-core calibration workload.

External-power detection during validation is also debounced so a transient
USB/charger status read cannot incorrectly cancel a valid stability run.

### Recovery evidence is preserved

A crash/coldboot recovery no longer interprets a transient charger-offline
reading as an intentional user cancellation.

Charger cancellation is now handled only by the boot wrapper, where power
detection is debounced and cancellation is allowed only on a normal boot.

### Fresh optimizer state

Beta 3 retains the Beta 2 fix that allows optimization to start correctly on a
completely fresh SD card.

## Real-device validation

The updated optimizer completed a full 8/8 OPP calibration on the Miyoo Flip
used for development, published the resulting profile, returned to FlipOS
normally, and left no pending trial or stock-only recovery latch.

The completed profile on that device is device-specific and is **not** shipped
as a universal voltage table. Every Flip calibrates its own CPU.

Automatic recovery from every possible silicon hard-lock condition cannot be
guaranteed, so FlipOS remains Beta software. The rollback and recovery
mechanisms continue to be treated as safety-critical.

### Clear update status

Checking GitHub while already on the current release now reports
**YOU'RE ON THE LATEST RELEASE** instead of presenting the already-installed
version as an update.

## Downloads

- **FlipOS-Fresh_Install.zip** — for a new SD-card installation.
- **FlipOS-Update.tar.gz** — for updating an existing FlipOS installation.

For compatibility with the Beta 2 updater, this release also includes a
temporary legacy-named **FlipOSUpdate.tar.gz** copy of the same update archive.
Future releases will use the clearer FlipOS-Update naming.

## Updating from Beta 1 or Beta 2

Connect to Wi-Fi, open the FlipOS update screen, check GitHub for updates, and
install Beta 3.

The update package is validated and staged before installation. As FlipOS is
still Beta software, keep normal backups of important saves.

## Please report issues

If you see an optimizer lockup, failed recovery, update problem, crash, failed
resume, game issue, Wi-Fi problem, battery/power issue, or anything else that
looks wrong, please report it:

https://github.com/AnthonyWCurtis/FlipOS/issues
