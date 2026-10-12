#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c"

text = SRC.read_text(encoding="utf-8")

required = [
    '#define TSADC_AUTO_CON 0x04',
    '#define TSADC_INT_EN 0x08',
    '#define TSADC_INT_PD 0x0c',
    'TSADC_TSHUT_POLARITY BIT(8)',
    'SHUT_GPIO_BITS',
    'SHUT_CRU_BITS',
    '#define CRU_GLB_RST_CON 0x00dc',
    'CRU_WDT_RESET_EXT BIT(7)',
    'CRU_PMU_SRST_WDT_DISABLE BIT(4)',
    '#define PMUGRF_GPIO0A_IOMUX_L 0x0000',
    'GPIO0A2_TSADC_SHUT_M1',
    'GPIO0A2_MUX_WRITE_MASK',
    'RK817_RECOVERY_MASK',
    'RK817_RECOVERY_VALUE',
    'SLPPIN_RST_FUN',
    'RK817_SLPPOL_L',
    '"rockchip,rk3568-cru"',
    '"rockchip,rk3568-pmugrf"',
    '"rockchip,rk3568-tsadc"',
    '"rockchip,rk817"',
    'of_find_i2c_device_by_node',
    'rk817->variant != RK817_ID',
    '"gpio"',
    '"otpout"',
    'module_param_cb(recovery',
    'set_cru_wdt_recovery(true)',
    'restore_cru_wdt_recovery()',
    'original_cru_wdt_reset_bits',
    'original_gpio0a2_mux',
    'original_rk817_recovery_bits',
    'validate_tshut_idle_low_active()',
    'pending & SHUT_GPIO_BITS',
    'auto_con & TSADC_TSHUT_POLARITY',
    'rk817_restart_active_low=%u',
]
for needle in required:
    assert needle in text, f"missing required safety behavior: {needle}"

for forbidden in [
    'COMP_SHUT',
    'hw-tshut-temp',
    '120000',
    '115000',
    '95000',
    'set_tshut_temp',
    'PMUGRF_WDT_FAILSAFE',
    'wdt_shut_reset_trigger_en',
    '0x00800080',
    '0x00800000',
    'original_cru_wdt_reset_ext',
    'set_cru_wdt_reset_extension',
]:
    assert forbidden not in text, f"forbidden/stale recovery behavior present: {forbidden}"

# TSADC route and every hardware register we change must be restored to the
# exact state observed when the module loaded.
assert 'val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);' in text
assert 'val |= original_route_bits;' in text
assert 'set_gpio0a2_mux(original_gpio0a2_mux)' in text
assert 'set_rk817_recovery_bits(\n        original_rk817_recovery_bits)' in text
assert 'val |= original_cru_wdt_reset_bits;' in text

# Watchdog recovery must both extend reset and allow watchdog reset to reach
# the PMU reset domain. Bit 4 is active-low, so arming clears it.
assert 'val |= CRU_WDT_RESET_EXT;' in text
assert 'val &= ~CRU_PMU_SRST_WDT_DISABLE;' in text
assert '(CRU_WDT_RESET_EXT | CRU_PMU_SRST_WDT_DISABLE)' in text
assert '(val & CRU_PMU_SRST_WDT_DISABLE)' in text

# RK3568 PMUGRF uses an upper-half write mask. Never rewrite unrelated IOMUX.
assert (
    'GPIO0A2_MUX_WRITE_MASK | mux' in text
), "GPIO0_A2 mux must use PMUGRF write-mask semantics"

# The TRM says a disabled TSHUT GPIO output is held low. That would instantly
# trigger RK817's active-low restart input. Therefore enable TSHUT output
# before muxing GPIO0_A2 to TSADC_SHUT_M1, and only make RK817 restart live
# after the mux is known-safe.
arm_begin = text.index('static int set_route(bool to_pmic)')
enable_route = text.index('val |= SHUT_GPIO_BITS;', arm_begin)
mux_m1 = text.index('GPIO0A2_TSADC_SHUT_M1', enable_route)
pmic_live = text.index('RK817_RECOVERY_VALUE', mux_m1)
assert enable_route < mux_m1 < pmic_live, (
    "unsafe PMIC recovery arm ordering"
)

# During teardown the PMIC input must be made inert before mux restoration.
restore_begin = text.index('static int restore_recovery_path(void)')
disable_pmic = text.index('set_rk817_sleep_disabled()', restore_begin)
restore_mux = text.index('set_gpio0a2_mux(original_gpio0a2_mux)', restore_begin)
assert disable_pmic < restore_mux, "unsafe PMIC recovery restore ordering"

# A partial failed arm must stay marked hazardous so module unload retries
# restoration rather than silently leaving PMIC/reset routing changed.
assert 'armed = true;\n            (void)hold_module_while_armed();' in text
assert 'try_module_get(THIS_MODULE)' in text
assert 'module_put(THIS_MODULE)' in text

print("PASS: TSADC/WDT -> RK817 PMIC restart recovery architecture")
