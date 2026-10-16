# FlipOS RK817 charging audit — 16 October 2026

**Scope:** Published `AnthonyWCurtis/FlipOS` `main` at `6d6eb28e93e1eee24cb9e0b23b64d3641c6bee19`, not the newer uncommitted MSI checkout or an observed hardware fault. Source files reviewed: `src/runtime/flip_charge_service.c`, `flip_charger.c`, `flip_power_policy.c`, `flip_battery.c`, their headers/tests; `tools/kernel/flip_rk817_charger/flip_rk817_charger.c`; `tools/device/updater-playtest`; `docs/ROADMAP.md`; RK817 datasheet v2.1, PDF pages 23–24, 41 and 124.

**Disposition:** source-level safety findings; **no changes to production charger code, PMIC, battery, Linux sysfs, kernel modules, or actual hardware**. Model-based counterexamples are NOT proof of a fault on the user's physical board.

## Architecture

1. Boot hook `tools/device/updater-playtest` inserts `flip_rk817_charger.ko` and launches `flip-charge-service`; it checks an unverified PID file with `kill -0` for an existing daemon.
2. One-second userspace loop reads `flip_battery_snapshot_t` from standard `power_supply` files, including `temp` in 0.1°C and AC/USB online state. The shared reader also accepts `/tmp/flipos-dev-...` temperature/external-power overrides.
3. Policy uses hysteresis: normal <40°C, warm at ≥40°C (cap request 1500mA, exit below 38°C), hot at ≥45°C (1000mA, exit below 43°C), extreme hot at ≥50°C and extreme cold ≤0°C (500mA request; transitions off extreme hot below 47°C, off extreme cold at ≥5°C). No current override is requested when external power is missing/false or temperature is unknown.
4. Kernel module reads/writes RK817 `PMIC_CHRG_OUT` I²C address `0xE4`, masking bits `[2:0]` for `CHRG_CUR_SEL`. RK817 datasheet v2.1 PDF p.124 confirms the full selector table including selector 7 = 500 mA. Upper bits of `0xE4` include POWERPATH_EN, charge-voltage selection, and thermal-foldback enable. RK817 v2.1 PDF p.41 describes a separate TS/thermistor-based battery-temperature pause, which requires correct board implementation/configuration to actually work.
5. Ordinary stop writes `-1` to 'restore' an earlier selector; only the stock driver/PMIC controls charge-enable, charge voltage and input current. The power-policy decision `charger_enabled=0` during `PAUSED_*` is never applied to the hardware. These labels must not be represented as an actual charger-off command.

## Findings, ranked

### P0 / high — `PAUSED_*` names do not disable charge

`flip_charger.c` explicitly maps both `PAUSED_COLD` and `PAUSED_HOT` to `target_ma=500`. The kernel helper writes selector 7 (500mA), **not** CHRG_EN. The autonomous RK817 TS temperature protection may stop charging if appropriately wired and configured, but the supplied public source and RK817 datasheet alone do not prove the board's TS wiring/configuration or physical cutoff. This is a **safety-acceptance gap**, not evidence of actual uncontrolled charging. Until verified, do not advertise FlipOS as stopping charging at 0 or 50°C; it only asks for the lowest current.

### P0 / high — remembered baseline can override a new, lower vendor limit

`flip_rk817_charger.c::apply_current_limit` saves the selector of the first override and thereafter sets `target_ma = min(remembered_baseline, requested_cap)`, without also bounding by the **current** selector. Reproducible modeled scenario:

- At module load, vendor charger selector=3 (2500mA).
- FlipOS requests warm cap 1500mA, latches restore selector=3 and programs selector=1 (1500mA).
- Vendor charger later programs selector=7 (500mA) for its own reason.
- The next once-per-second FlipOS warm sample uses the saved 2500mA baseline rather than today's 500mA selector, and programs selector=1 (1500mA). The helper has **raised** the actual selector and defeated the newer restrictive decision.
- Writing `-1` subsequently restores the saved 2500mA selector even if the vendor has since reduced current. The last write is also a potential elevation in charger current, contradicting the module's description as a *cap only*.

Fix requires coordinated charger ownership. At minimum never program a positive cap above the currently read selector; release must not blindly restore a stale old selector. Merely changing to min(current, requested) can strand the device at a low setting indefinitely; the right hand-off to the stock driver requires investigation and physical testing.

### P0 / high — concurrent read/modify/write of multifunction PMIC register

The module issues `i2c_smbus_read_byte_data` then `i2c_smbus_write_byte_data` for **all eight bits of `PMIC_CHRG_OUT`** with only bits 0–2 changed in its copy. If the stock RK817 driver updates power-path, voltage, or foldback bits between the two transactions, this direct I²C whole-byte write can lose the stock update. There is no common mutex/regmap lock shown in FlipOS module; `current_set()` also has no module-local serialization. Use kernel-owned synchronized masked writes and avoid competing direct control with Rockchip's charger driver. Do not deploy a speculative register rewrite.

### P1 / medium–high — sensor failure releases thermal cap

`flip_battery_read()` returns 0 with validity flags even when `temp` cannot be read; `flip_charge_policy_evaluate()` treats missing temperature as NORMAL; `flip_charger_update()` writes `-1` on transition to unrestricted mode. Thus a temporary sensor/read error can release a previously applied hot limit. Under actual hardware TS cutoff it might still be safe, but FlipOS itself fails to retain its prior restriction. Decide a policy for bounded stale samples, fault alerts, and non-escalating fail-safe behaviour; verify over hot-plug and suspend/resume.

### P1 / medium–high — development overrides affect production charge policy

`flip_battery.c::flip_battery_read` accepts `/tmp/flipos-dev-battery-temp-override` and `/tmp/flipos-dev-external-power-override` without a charger-specific opt-out; the live charge service uses this same snapshot. Leftover/misplaced RAM override files could change thermal decisions without a real sensor reading. Isolate test injection from charge safety inputs (build-time gated test mode, or a strict physical-reader API), and test it.

### P1 / medium — on apply failure service tries releasing restriction

In `flip_charge_service.c`, a failed `flip_charger_update` logs an error then calls `flip_charger_restore`, potentially replacing a restrictive cap with the saved higher selector. A transient sysfs or I²C error during a hot state must not trigger unreviewed increases. Error-handling should preserve conservative state and report the fault until there is verified safe recovery.

### P2 / lower — observability and service lifecycle gaps

- The `applied_current_ma` and `cap_ma` log fields are **requested** caps, not a measurement of effective charge current. The module's stock selector is a boot-time sample, not a promised board/battery limit.
- `charge.log` is opened append-only and has no bounded rotation in this service; a long-running/flapping installation can grow it.
- Startup's PID-file check uses `kill -0`, which can match a reused/unrelated process and skip the service; there is no supervision/restart policy in the reviewed boot script.
- Existing tests check pure temperature hysteresis and mocked sysfs writes; architecture tests are string checks only. They do not mock concurrent vendor writes, I²C errors, missing temperature with an existing cap, extreme-temperature actual charge cutoff, charger driver unload/restore, or repeated suspend/hotplug. No PMIC kernel-unit stress or physical temperature-range acceptance is shown in the public sources reviewed.

## Current release's charger module identity

The existing release packaging still sources `flip_rk817_charger.ko` from the **v0.1.0-beta.4** legacy update archive, instead of compiling the module from the current checked-out C source. `tools/kernel/legacy-module-lock.json` pins module SHA-256 `ee2b58290d49a62602435086c39d28bbd9c4fb0145fa81034dab413780050bb4`, base kernel `5.10.160`, and vermagic `5.10.160 SMP mod_unload aarch64`. The release CI validates these inputs, but hash and vermagic are not proof of real board/driver equivalence. **Our code observations apply to the published helper C source; whether the shipped Beta 4 binary has identical semantics needs source provenance/disassembly or documented matching-build evidence.** Never silently replace that prebuilt `.ko` with a speculative new kernel module. The existing lock and device-side ABI checks must be reconciled first.

## Recommended implementation sequence

1. **Non-invasive physical inventory (read only):** identify exact PMIC, Rockchip charger driver and `regmap` ownership, device-tree TS battery thermistor connection/configuration, effective PMIC charging/status register vs sysfs, kernel ABI and installed module provenance. No testing at hazardous battery temperature. Do not disconnect thermal protection or modify CHRG_EN during this inventory.
2. **Uncouple safety telemetry and test overrides:** add a real hardware-only charging snapshot. Host-test loss-of-temperature, unplug, high-temperature hysteresis and preserved restrictions without changing charging hardware.
3. **Fix cap/release semantics in the kernel helper:** coordinate with the stock driver, serialize appropriately, protect unrelated register bits and prevent any increase over vendor-imposed restrictions. Add a fake-register transaction harness with synthetic vendor write races and failure injection.
4. **Rename the misleading extreme modes in UI / logs** unless true shutdown can be formally designed and validated without compromising power-path safety. Prefer words such as 'MIN_CURRENT_COLD' / 'MIN_CURRENT_HOT' instead of 'PAUSED' until physical cutoff proven. Preserve compatibility for historical log parsers if changing identifiers.
5. **Long-soak reliability:** bound logs, PID lifecycle supervision, repeated USB connect/reconnect, charge-while-playing, suspend/resume, 24 h monitoring, thermal and charge current trends, measured current with safe instrumentation.
6. **Acceptance:** run host CI on a new MSI integration worktree (preserve Oct 15 work); build matching RK817 kernel module with pinned ABI; physical tests only with explicit approval, safe room-temperature scenarios and backup of SD/user data. Never conduct intentional dangerous overheat/cold charging experiments.

**Release disposition:** Do not advertise autonomous safe charging cutoff or fully stable charge-while-playing until those physical gaps are resolved. Do not infer battery health, electrical safety, or current draw from software `cap_ma` logs alone.