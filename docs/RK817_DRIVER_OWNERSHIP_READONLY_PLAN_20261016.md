# RK817 charging driver ownership: read-only baseline plan (16 October 2026)

**Status:** draft engineering research and disposable-host tests. **No charger/PMIC modifications, no device access, no upload.**

## Why the next step is inventory, not a kernel patch

FlipOS's published kernel helper (`tools/kernel/flip_rk817_charger/flip_rk817_charger.c`) uses uncoordinated SMBus whole-byte read/modify/write to RK817 `PMIC_CHRG_OUT` register `0xE4`, and restores an older `CHRG_CUR_SEL` selector. This may override the vendor driver's current reduction or overwrite a concurrent change to unrelated bits. The helper is not the same as the stock charger driver and neither one can be assumed to own the other's register operations.

Public upstream Linux RK817 charger code and older Rockchip/BSP vendor code have different interfaces and history. **Neither establishes the actual implementation of the user's stock kernel**. Check deployed driver binary/kernel source + device tree before selecting a locking or power-policy design.

The current release packaging checks `tools/kernel/legacy-module-lock.json`. It pins `flip_rk817_charger.ko` SHA-256 `ee2b58290d49a62602435086c39d28bbd9c4fb0145fa81034dab413780050bb4`, base kernel `5.10.160`, expected vermagic `5.10.160 SMP mod_unload aarch64`, and a legacy **Beta 4** module archive. The exact installed device module has **not** been verified in this offsite review, and matching module hash by itself does not prove PMIC thermistor behaviour.

Source references for *comparison only*, not identification of the deployed stock driver:
- Rockchip vendor-era `drivers/power/rk817_charger.c` (different code and API family): https://github.com/54shady/qop_kernel/blob/master/drivers/power/rk817_charger.c
- Modern upstream `drivers/power/supply/rk817_charger.c`, which uses the parent PMIC regmap: https://codebrowser.dev/linux/linux/drivers/power/supply/rk817_charger.c.html
- Miyoo Flip research wiki, with separate hardware investigations and power-off handling: https://github.com/Zetarancio/Miyoo-Flip-Mainline-Linux-Reverse-Engineering

## Manual read-only data collection (after MSI/device available)

`tools/device/flip-charge-readonly-inventory.sh` is an **uninstalled development helper**. It prints an allowlisted local report; the output contains only selected power_supply observations, module parameters, driver/module names, the kernel release and an SD-card module SHA-256. It never writes to `/sys`, `/dev`, PMIC I²C, CPU voltage settings or storage.

When you are physically present and have accepted the test, copy the script to a disposable location on the Flip or run its content through an explicitly authorised shell. Do not enable remote unattended experiments. Run in ordinary room-temperature conditions. Save the output on the host for manual review; it is **not sent automatically**. Do not alter USB/power while an update, save transaction or ROM shutdown is in progress.

The `FLIP_CHARGE_PROBE_ROOT` variable supports **host synthetic fixtures** only (no actual device). Unit tests exercise the script against fake paths in `TemporaryDirectory`.

## Interpretation limits

- `module_limit_ma` and `module_stock_ma` are helper settings, not measured battery current.
- `battery_current_ua` is a current sensor reading from the stock power_supply driver, not proof of a selected PMIC register limit or thermal safety.
- Missing files are reported as `unavailable`. A missing path can reflect a different kernel configuration, driver name, device tree, module state or absent hardware; it does not prove a fault.
- Present platform driver names do not prove binding, ownership of the PMIC or verified thermal cutoff.
- A matching `.ko` SHA-256 is a provenance hint, not physical/electrical acceptance. Actual loaded-module identity requires further kernel and build evidence.
- The stock `BAT_TMP_ERR`/thermistor and charger-enable behaviour must be verified in the *exact board's* driver/DT implementation. Do not infer that a software request of **500mA** means charging is off.
- Host tests cannot simulate I²C arbitration, PMIC charge sequencing, Linux regmap locking or power-loss reset. They **must not** justify a speculative module change.

## Host tests and release boundaries

```sh
sh -n tools/device/flip-charge-readonly-inventory.sh
python3 -m unittest discover -s tests -p 'test_charge_readonly_inventory.py' -v
make verify
```

Before any kernel/PMIC change:
1. Preserve the MSI's newer uncommitted Save Manager/recovery development; reconcile in a **separate** worktree.
2. Compare installed module SHA and `uname -r` with the pin; inspect stock kernel driver's *actual* source or symbols, parent `rk808` regmap, device-tree charger/battery TS wiring and input-current policy.
3. Determine exclusive ownership or a coordinated, serialized masked-write interface. Abandon a runtime override if this cannot be proven.
4. Only after code review and explicit approval use a disposable-save, reversible, **room-temperature** device test. No raw PMIC writes, forced charging at temperature extremes, NAND/MTD edits or unattended power tests.
5. Treat thermal cutoff and stable charge-while-playing as open acceptance gates.

The hardware-safety decisions above are not automated by this inventory tool. Keep the separate charger audit #50 and userspace hardening #51 draft and unmerged until these prerequisites are met.
