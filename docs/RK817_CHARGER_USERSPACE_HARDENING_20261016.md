# FlipOS RK817 charger userspace hardening (16 October 2026)

**Status: draft code / synthetic CI only. NOT authorised for device deployment, not a charging-safety certification.**

## Reviewed baseline

Published `main` SHA `6d6eb28e93e1eee24cb9e0b23b64d3641c6bee19`, with the independent findings in draft charging audit PR #50. The MSI has a newer unpublished development checkout which must be reconciled before implementation is considered current.

The narrow charger helper writes RK817 `PMIC_CHRG_OUT` (0xE4) field `CHRG_CUR_SEL[2:0]`. Userland asks the helper for stock release (-1), or supported current selections. It does **not** disable charging, set battery charging voltage, or change power-path routing. Even the `PAUSED_HOT` and `PAUSED_COLD` states request 500 mA, not 0 mA.

## Conservative changes in this branch

1. `flip_battery_read_hardware` reads the same validated sysfs fields as `flip_battery_read`, **without** honouring development-only `/tmp` overrides for temperature, supply-online status, voltage or percentage. Only `flip-charge-service` switches to the hardware-only API; other development/telemetry users retain the existing override behaviour.
2. When input power is **unknown**, or input power is known online while battery temperature is **unknown**, `flip_charger_update` leaves the last programmed cap unchanged and does not advance its policy. A verified unplug is allowed to release the cap. Reliable cool-temperature data is needed before a warm/hot thermal cap is lifted.
3. If setting a charge cap fails, `flip-charge-service` logs the error and retries on the next sample, rather than actively invoking `flip_charger_restore` to increase the allowed current. Unknown-input warnings are reported once per consecutive period.
4. Test fixtures exercise all development-override bypass paths, missing thermistor readings, unknown external-power status, safe unplug release and ordinary recovery of the thermal policy. The architecture guard prevents regression to stock-release-on-error handling.

## What this **does not fix**

- **P0 driver ownership:** The kernel helper can still write `PMIC_CHRG_OUT` using an uncoordinated whole-byte SMBus read/modify/write and save an outdated `restore_selector` while the stock charger driver is changing current. This branch does **not** change the kernel module, register accesses or module parameters. A proper solution requires inspection of the exact deployed Rockchip kernel charger driver, its regmap/locking and DT configuration, and proof of ownership on this board.
- **Extreme temperature:** 500 mA is a supported current, **not** a true charging pause. Never claim the software stops charging at 0 C or 50 C. Actual suspension must come from independently verified stock RK817 charger/thermistor protections.
- **Sensor hardware faults:** An invalid measurement from sysfs might not capture every thermistor fault; a plausible but inaccurate reading cannot be automatically detected. A cold boot with missing temperature leaves stock charging in control; the new code can only retain a **previously installed** cap.
- **Service loss and shutdown:** The existing graceful-exit handler still releases its cap. Its interactions with stock charging, service supervision and reboots need a separate design review. Neither this branch nor the host tests prove charging safety if the daemon terminates.
- **Update runtime:** The module in the Beta release package may be a pinned legacy binary. Changing this userspace service does not alter the deployed module's internal behaviour.

## Safe verification order

1. On a disconnected developer host, run `make test-battery test-charger test-power-policy test-architecture` and `make verify`. GitHub CI should separately build the real ARM64 `flip-charge-service` and perform the release-package dry run. Run tests only against fake `/tmp` sysfs roots.
2. Once the MSI reconnects, inspect its newer code and exact deployed `flip_rk817_charger.ko` SHA/vermagic *read-only*. Do not overwrite the existing tree.
3. On the real Miyoo Flip, begin with read-only temperature, power_supply and logging measurements in ordinary room-temperature conditions. Compare sysfs status/current against the stock driver and the actual PMIC hardware registers with the owner-driver coordination understood. **Do not** write raw PMIC registers, heat or chill the battery to exercise 0 C/50 C thresholds, disable temperature protections, or run unapproved unattended stress tests.
4. Only after a reviewed, hardware-safe driver-coordination design and explicit controlled-test authorisation should any charging-current writes be considered. Preserve stock recovery and power-cycle escape routes.

**Release decision:** charging safety remains **not accepted** on hardware. Keep this PR draft and separate from the already-tested stability integration PR #49 until MSI reconciliation and physical acceptance.
