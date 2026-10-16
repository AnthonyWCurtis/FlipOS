# Dynamic CPU policy: invalid FPS sample safety

**Status:** Host/CI regression candidate (16 October 2026). **No voltage adjustment and no hardware acceptance claimed.**

The existing `flip_cpu_dynamic_sample` algorithm receives approximately one performance window per second. Previously, it returned early for FPS values below 1, but allowed NaN and infinite FPS values through floating-point comparisons. Those comparisons evaluated false for NaN, making a missing FPS sample count as a **healthy window**. After enough nominally healthy windows, the controller could probe a lower CPU operating point without uninterrupted timing evidence. The original ordering also consumed warmup/cooldown/probe windows before detecting invalid timing samples.

This change validates both measured and target FPS using `isfinite` and the existing minimum-FPS check before touching window bookkeeping. An invalid sample clears consecutive healthy and bad-window streaks, but does not consume warmup, cooldown or lower-probe windows and does not perform a sysfs clock write. A subsequent valid sample resumes normal adaptive behaviour.

The existing **valid-sample** rules remain the same: severe slowdowns and audio drops raise one frequency step immediately, repeated marginal pressure raises after two windows, sustained smooth execution probes down one step after fifteen healthy windows, and the allowed maximum and learned floor remain authoritative. Neither power policy nor hardware frequency/voltage tables were changed.

Added focused tests cover invalid samples during warmup, interruption of an almost-complete lowering streak, interruption of two-window pressure hysteresis, and an audio-drop event with invalid versus valid FPS evidence. The existing tests continue to cover FFVI-style slowdowns, frame-late bursts, audio drops, boundaries, and learning a failed lower probe.

**Verification on PR:** `make test-cpu-dynamic`, `make verify`, the standard CI ARM64 build, package checks and `git diff --check`. All tests use a fake `FLIP_CPU_POLICY_ROOT` under `/tmp`, not actual CPU sysfs.

**Unresolved:** A synthetic test cannot quantify real power savings, temperature or display hitches. Reconcile with the newest MSI checkout; then perform identical-workload, disposable-save, USB and unplugged power baselines. Do not merge or deploy based only on CI.
