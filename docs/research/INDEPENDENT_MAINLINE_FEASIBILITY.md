# Independent mainline Linux feasibility — Miyoo Flip / FlipOS

**Research date:** 2026-10-14  
**Decision:** **Investigate as a separate research branch; do not replace the Beta 6.1 kernel.**  
**Status:** Source/documentation feasibility review only; no mainline kernel built, booted, or hardware-tested by FlipOS.

## Engineering constraints

- **Original FlipOS board-support implementations.** We may learn documented hardware facts and consult manufacturer datasheets, the Linux driver and Device Tree specifications, and public behaviour reports. We must **not copy Zlyme/ROCKNIX board-specific drivers, DTS, bootloader changes or patch files into FlipOS**, nor rebrand them as ours.
- **Do not carry permanent downstream modifications to in-tree Linux or U-Boot source.** Prefer existing mainline drivers, a newly authored stand-alone Flip board DTB, standard userspace APIs and independently written self-contained driver modules. If a missing feature requires changing upstream kernel internals, plan an original upstream submission and treat full support as blocked until acceptance/availability; do not silently import an external patch.
- Kernel, GNU toolchain, Buildroot (if eventually chosen), libc, Gambatte and mGBA are third-party open-source components. Building on the Linux kernel necessarily **uses other people's open-source code**. The original-work rule applies to *our new board-specific implementation* and does **not** cancel applicable copyright notices, licences, GPL obligations or attribution.
- **Absolutely no internal NAND/MTD, SPL, preloader, MASKROM-erase or boot-order write during research.** Preserve current SD-first Beta 6.1 and stock recovery. A mainline test may proceed only when a proven non-destructive boot route has been demonstrated independently, with recovery instructions.
- Evidence from another distro is not FlipOS hardware acceptance. Do not infer benchmark or battery improvements from a kernel version number.

## Sources and boundaries

**Primary existing FlipOS evidence:** [ROADMAP](../ROADMAP.md), [performance test plan](../PERFORMANCE_TESTING.md), [optimizer calibration](../OPTIMIZER_CALIBRATION.md), and project-uploaded manufacturer RK3566 V1.5, RK817 V2.1, RK860 V1.7, RK3566 hardware guide and Fn-Link 6233B-UUB V2.1 datasheets.

**Independent Linux base:** upstream Linux ARM64 [RK3566 DT include](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/rockchip/rk3566.dtsi), [generic DRM/VOP2](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/rockchip/rockchip_drm_vop2.c), and related standard drivers.

**External public behaviour reports, *not implementation templates*:** [Zlyme README](https://github.com/Zetarancio/zlyme/blob/main/README.md), [Zlyme architecture](https://github.com/Zetarancio/zlyme/blob/main/docs/ARCHITECTURE.md), [hardware wiki](https://github.com/Zetarancio/Miyoo-Flip-Mainline-Linux-Reverse-Engineering) (especially board wiring and display facts). Independent reproduction against datasheets and target device is required.

Verified against upstream `torvalds/linux` **master** on 2026-10-14: the generic `rk3566.dtsi` and Rockchip VOP2 DRM driver exist, but `arch/arm64/boot/dts/rockchip/rk3566-miyoo-flip.dts` is **not in that tree**. This is a current source check, not proof that no draft upstream submission exists.

Zlyme's architecture describes Linux 7.0.2 **with its own Linux/bootloader patch series, non-upstream Wi-Fi driver, custom board DT and custom board drivers**. Its own audit calls the shipped Linux patch stack **16 patches**. Therefore its successful boot proves mainline-as-a-base is viable, **not** that an unmodified mainline tree supports the whole Flip.

## Initial compatibility matrix

| Component | Upstream foundation | Flip-specific remaining work without copied patches | Risk |
| --- | --- | --- | --- |
| RK3566 A55 CPU, clocking, thermal and base OPPs | ARM64 Rockchip SoC DTS and generic CPUFreq/thermal exist | Author Flip regulator wiring/DTB, compare supported OPP table against stock profile; new runtime API adapter | **Medium** |
| Two SD slots, basic USB, GPIO, regulators | Standard MMC/USB/GPIO/regulator kernel frameworks | Independently define exact Flip supplies, pinmux/PHY/USB routing and slot aliases, verify on retail boards | **Medium–high** |
| 640x480 2-lane MIPI DSI display | DRM/KMS VOP2 and MIPI DSI host exist | The Flip LCD needs its own **original** panel init/sequencing implementation; generic DRM does not initialize its vendor command sequence alone | **High** |
| Mali-G52 graphics | In-tree Panfrost is available; vendor Mali is a separate stack | Choose supported GPU userspace, port/test FlipOS shared Video/GPU Host; avoid assumed Vulkan support | **Medium–high** |
| RK817 codec, battery, PMIC; RK8600 CPU rail | Core Rockchip PMIC, ALSA and regulator frameworks exist | Our authored board wiring, charger behaviour, safe power-off/suspend; stock optimizer module ABI must **not** be reused | **High** |
| Face buttons, lid, volume/power | Standard gpio-keys, input events, RK817 power key | Original Flip board DT/input policy; physical polarity/wake matrix | **Medium** |
| Analog sticks and rumble | UART, Linux input and PWM frameworks exist | Original Miyoo serial-stick protocol implementation (kernel or userspace as justified), calibration/haptic and safe resumption | **High** |
| Wi-Fi/Bluetooth (Fn-Link 6233B-UUB / RTL8733BU-CG) | USB, Bluetooth/rfkill and networking frameworks exist | RTL8733BU Wi-Fi is described by external sources as **out-of-tree**; an original fully working WLAN driver plus power control would be major engineering work. Hardware datasheet identifies module/chip but does not supply a Linux Wi-Fi driver | **Very high** |
| Deep suspend / prolonged battery standby | Linux suspend/regulator infrastructure exists | Mainline-specific RK3566 BL31/SIP coordination, rail states, RK817 off-state drain and rollback; Zlyme still uses kernel-source patches for this | **Very high** |
| DDR DMC scaling | devfreq/event infrastructure exists | Dynamic RK3566 memory scaling is not a standard ready-made driver in the reviewed stack; write own module or defer | **High / optional** |
| Booting mainline on retail Flip | RK3566 BootROM/U-Boot ecosystem exists | **Critical:** FlipOS Beta 6.1 currently launches userspace with the stock Linux kernel. A new Linux image requires a boot route not delivered by that normal stock-kernel userland path. Zlyme's documented route involves an internal preloader modification — **not approved for FlipOS**. Research a validated RAM-only/otherwise nondestructive route or stop | **Critical** |

### CPU performance trap

Upstream's reviewed `rk3566.dtsi` CPU OPP table tops out at **1800 MHz**. FlipOS's current stock/BSP measurements and optimizer cover **1992 MHz**. This does **not** by itself prove 1800 MHz would be too slow for GB/GBC/GBA, but switching kernels is **not automatically a performance upgrade**. Port the dynamic-clocking policy only after comparing stable frame-rate, audio and energy use under the new OPP and governor behaviour. No undervolt experiments in mainline bring-up.

### Display specificity

Hardware reference reports the physical LCD as 640x480, two-lane RGB888 MIPI DSI (LMY35120-20p panel). The vendor initialization commands, timings, regulator sequencing and backlight must be validated independently. Linux's generic display-controller driver is not a substitute for the panel-specific initialization. The hardware wiki records a mainline bring-up with a patched panel driver; our rule rules out copying that patch, so **this is a new original-driver task**.

### No downstream patches: what it really requires

This is possible as a **policy for our maintained kernel tree** (use pristine upstream + own DTB + out-of-tree, separately built modules), *not* a guarantee of complete functionality. Several features require changes to code currently inside the kernel, notably deep-suspend integration and PMIC power-off behaviour in the available implementation. If upstream does not provide a suitable interface, the choices are to (1) omit/defer the capability, (2) author and propose a maintainable **upstream** addition (a patch during review, but no permanent downstream fork), or (3) stop the migration. We will **not** claim a patchless full-function FlipOS while relying on unpublished workarounds.

## Go/no-go research sequence — never in production main

0. **Source-only checklist (can do remotely now):** pin upstream stable/LTS candidate and compiler; enumerate existing drivers/DT bindings; write original board power/pin/clock inventory from manufacturer datasheets and safe device observation; document third-party licence obligations. **No Zlyme code copied.**
1. **Boot safety gate (requires hardware/recovery review):** demonstrate an independently proven **non-destructive, no-NAND-write** method to load a test kernel. If impossible under our constraint, **STOP**. No change to stock SPL, boot order or recovery strategy.
2. **Minimal prototype:** experimental read-only/minimal userspace boots to UART/serial log and reports actual CPU/PMIC/MMC. No emulator or network requirement. Record DTB+kernel hashes and boot logs.
3. **Display + input:** original panel driver and device tree, then safe physical button and stick mapping. Fail closed if regulators or power domains do not match measurements.
4. **Userland proof:** compile existing FlipOS userspace against the new kernel interfaces, run DRM/ALSA/GB/GBC/GBA and save/state tests, compare real performance to Beta 6.1 baseline.
5. **Power/network drivers:** Wi-Fi/BT, suspend, charging, off-state drain, DMC only after independent test plans. No experimental CPU voltages. Do not ship until device safety, full feature parity and release recovery are accepted.
6. **Promote only if evidence wins:** identical or better reliable functionality, measurable performance/energy benefits or compelling maintenance savings, reproducible builds and safe OTA/rollback. Otherwise keep the stable kernel.

**Current go/no-go:** **GO** for independent *research and source-only prototype planning*; **NO-GO** for kernel cutover, public mainline build claims or automatic NAND/bootloader changes.

## Legal/provenance and originality checklist

1. Do not copy or mechanically translate third-party Miyoo board drivers, DT files, patches, boot scripts, firmware binaries or proprietary panel initialization data.
2. Write our own design from public interface specifications, datasheets, measured electrical facts and independently collected traces. Hardware facts are not a blanket licence to redistribute someone else's source.
3. Keep authorship/commit history and identify upstream OS, drivers and tools by name. Follow licences and provide corresponding source where required.
4. Do not use a new proprietary blob or third-party driver without a separate documented dependency/licensing decision. Original-written RTL8733BU Wi-Fi may be economically impractical; this is a legitimate **stop criterion**, not something to hide.
5. Do not experiment on user saves, production power settings, preloader or NAND.

## Research conclusion

**A mainline-based FlipOS is technically plausible, because an independent implementation demonstrates the major RK3566 peripherals can be supported. However, under a strict no-copied-code, no-permanent-kernel-patches and no-NAND-changes policy, it is not yet a viable replacement for the current fully featured Beta.** The first practical issue is proving a non-destructive boot method; then independently solving panel + input, Wi-Fi and deep suspend. This is a separate long-term engineering project, not a feature to add to the next beta.
