# FlipOS development verification

FlipOS builds host-side tests on the development machine and cross-compiles
release software for the Miyoo Flip (RK3566). Do not confuse these checks with
real-device hardware acceptance.

## Development checks (no Miyoo Flip required)

From a clean checkout with the host compiler, Python 3, and AArch64 cross
toolchain installed:

```sh
make test-host       # shared 54-target host regression suite
make test-architecture
make verify          # host checks, architecture checks and ARM64 builds
```

`make verify` writes details to `/tmp/flipos-verify.log` and reports
`VERIFY: PASS` or `VERIFY: FAIL`. It deliberately does **not** build a
release archive or download third-party kernel modules.

## Release-package validation (trusted modules required)

```sh
# First provide these four kernel-ABI-matched modules in build/:
# flip_opp_voltage.ko, flip_opp_runtime_profile.ko,
# flip_tsadc_recovery.ko, flip_rk817_charger.ko
make verify-package
```

`verify-package` fails immediately if any module is absent or empty and then
builds release userspace before running the archive smoke test. **Presence
alone does not authenticate a module**: verify the expected SHA-256,
provenance, kernel ABI and `vermagic` against the intended target kernel
before using any module in release packaging. Never use placeholders or
arbitrary modules to make the test pass.

The release jobs are responsible for their separately reviewed module
provisioning and publish process. Passing GitHub Actions is not a substitute
for running the real-device regression playbook.

## What is not covered

Development verification does not prove safe undervolting, suspend endurance,
battery life, real GPU cold-start timing, recovery from update interruption or
RTC/Wi-Fi behaviour on the physical Miyoo Flip. Track these separately in
[the hardware regression playbook](UNATTENDED_REGRESSION_PLAYBOOK.md).
Do not touch the internal NAND/MTD or treat an experimental CPU calibration
test as safe for unattended hardware execution.
