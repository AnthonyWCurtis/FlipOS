# Miyoo Flip Hardware Reference

This file records hardware identified from the live Miyoo Flip v3 device tree,
kernel logs and bound drivers. Exact physical ICs are separated from RK3566
integrated blocks so the project documentation pack can focus on the documents
that matter.

## Confirmed discrete ICs

| Function | Confirmed part | Evidence | Documentation to collect |
| --- | --- | --- | --- |
| Main SoC | Rockchip RK3566 | Board model / live DT / kernel | RK3566 Datasheet; RK3566 Technical Reference Manual; RK3566 Hardware Design Guide / EVB2 hardware reference |
| PMIC / charger / fuel gauge / RTC / audio codec | Rockchip RK817 | I2C 0x20, compatible `rockchip,rk817` | RK817 Datasheet Rev 2.1 or newer |
| CPU external buck regulator | Rockchip RK8600 family | I2C 0x40, compatible `rockchip,rk8600`; kernel `rk860x-regulator` | RK860 / RK860x regulator datasheet; RK860x Linux binding/driver notes |
| Additional external buck regulator | TCS TCS4525 | I2C 0x1c, compatible `tcs,tcs4525` | TCS4525 datasheet / register map |
| Wi-Fi | Realtek RTL8733BU | live DT `wifi_chip_type=rtl8733bu`; vendor driver logs | RTL8733BU datasheet / hardware design guide; Realtek vendor Linux driver documentation / release notes |
| SPI NAND flash | exact model not yet identified | SFC node `spi-nand` at `spi@fe300000/flash@0` | Exact NAND datasheet after JEDEC/READ-ID capture |
| USB / accessory MUIC | exact model not yet identified | I2C 0x30, compatible `gi,muic` | Exact MUIC datasheet after driver/board identification |
| LCD panel | exact model/controller not yet identified | DSI two-lane `simple-panel-dsi`; 68x121 mm DT size | Exact panel/controller datasheet after panel ID / init-sequence identification |
| LPDDR memory | exact part not yet identified | RK3566 memory controller / board RAM | Exact LPDDR4/LPDDR4X datasheet after package marking or DDR-init identification |

## RK817 reset wiring confirmed on this Miyoo board

The live DT exposes four RK817 pinctrl states:

- `default`
- `pmic-sleep`
- `pmic-power-off`
- `pmic-reset`

The SoC side of the PMIC sleep/reset path is **GPIO0_A2**.

- `soc_slppin_slp`: bank 0, pin A2, mux function 1
- `soc_slppin_gpio`: bank 0, pin A2, GPIO function
- `pmic-reset`: combines `soc_slppin_gpio` with RK817
  `rk817_slppin_rst` / `pin_fun3`

Live RK817 `SYS_CFG3` was read as `0x18` through the kernel-owned
`/sys/rk8xx/rk8xx_dbg` regmap interface:

- SLEEP pin function = reset
- reset polarity = active low
- RST_FUN = 0 = full PMIC/device reset

A stock-voltage `DEV_RST` test using `SYS_CFG3 0x18 -> 0x1c` removed the
device from USB immediately. It did not autonomously return to USB within the
observation window, so direct RK817 `DEV_RST` is not yet accepted as the
consumer recovery path.

## RK3566 integrated blocks visible/enabled

These are not separate chips and should primarily be documented through the
RK3566 TRM:

- Quad Cortex-A55 CPU cluster / DVFS / OPP
- Arm Mali Bifrost GPU (RK3566 class: Mali-G52 family)
- VOP display controller
- MIPI-DSI controller
- HDMI controller
- USB2 PHYs
- USB3/DWC3 controllers
- DesignWare hardware watchdog at `0xfe600000`
- SAR ADC
- PWM controllers
- SFC / SPI flash controller
- NAND controller
- SDHCI / eMMC/SD controller
- UART, I2C, SPI and I2S controllers
- Rockchip PMUGRF/GRF/syscon and reboot-mode registers
- thermal / clock / reset / power-domain controllers

## Documentation pack — priority order

### P0 — needed for current power/reset development

1. **Rockchip RK3566 Technical Reference Manual**
2. **Rockchip RK3566 Datasheet**
3. **Rockchip RK3566 Hardware Design Guide / EVB2 schematic or hardware manual**
4. **Rockchip RK817 Datasheet Rev 2.1+**
5. **Rockchip RK860 / RK860x regulator datasheet**
6. **TCS4525 regulator datasheet**
7. **RTL8733BU datasheet / hardware design guide / vendor driver release notes**

### P1 — collect once exact part numbers are identified

8. Exact SPI-NAND datasheet
9. Exact MIPI-DSI LCD panel/controller datasheet
10. Exact `gi,muic` datasheet
11. Exact LPDDR4/LPDDR4X memory datasheet

### P2 — useful supporting references

12. RK3566 EVB2 user/hardware manual
13. Linux DT bindings for RK817, RK860x/FAN53555-family regulators, DWC watchdog
14. Realtek RTL8733BU vendor Linux driver source / release notes
15. JEDEC SPI-NAND command/reference material

## Identification work still required

- Capture SPI-NAND manufacturer/device ID from a healthy boot.
- Identify `gi,muic` by driver source, register signature or PCB marking.
- Fingerprint the DSI panel from its init sequence and/or physical panel marking.
- Identify LPDDR package/vendor from bootloader DDR training output or board
  package markings.
