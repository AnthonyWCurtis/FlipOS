#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c"

text = SRC.read_text(encoding="utf-8")

required = [
    '#define TSADC_INT_EN 0x08',
    'SHUT_GPIO_BITS',
    'SHUT_CRU_BITS',
    '#define PMUGRF_SOC_CON3 0x010c',
    'PMUGRF_WDT_FAILSAFE BIT(1)',
    'PMUGRF_WDT_FAILSAFE_WE BIT(17)',
    '#define CRU_GLB_RST_CON 0x00dc',
    'CRU_WDT_RESET_EXT BIT(7)',
    '"rockchip,rk3568-pmugrf"',
    '"rockchip,rk3568-cru"',
    'pinctrl_lookup_state(',
    '"gpio"',
    '"otpout"',
    'module_param_cb(recovery',
    'if (armed)\n        set_route(false);',
    'restore_pmu_wdt_failsafe()',
    'restore_cru_wdt_reset_extension()',
]
for needle in required:
    assert needle in text, f"missing required safety behavior: {needle}"

# Thermal protection remains untouched. This helper only changes shutdown/reset
# routing and watchdog fail-safe controls.
for forbidden in [
    'COMP_SHUT',
    'hw-tshut-temp',
    '120000',
    '115000',
    '95000',
    'set_tshut_temp',
]:
    assert forbidden not in text, (
        f"recovery helper must not alter thermal threshold: {forbidden}"
    )

# TSADC restore must preserve unrelated INT_EN bits.
assert 'val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);' in text
assert 'val |= original_route_bits;' in text

# PMU_GRF must use the documented upper-half write-enable bit so only SOC_CON3
# bit 1 is changed.
assert 'write = PMUGRF_WDT_FAILSAFE_WE;' in text
assert 'write |= PMUGRF_WDT_FAILSAFE;' in text
assert 'writel_relaxed(write, pmugrf_regs + PMUGRF_SOC_CON3);' in text

# CRU_GLB_RST_CON is ordinary R/W on this board. It must be changed by
# read-modify-write and restored from the originally observed bit value.
assert 'val = readl_relaxed(cru_regs + CRU_GLB_RST_CON);' in text
assert 'val |= CRU_WDT_RESET_EXT;' in text
assert 'val &= ~CRU_WDT_RESET_EXT;' in text
assert 'val |= original_cru_wdt_reset_ext;' in text

# The dangerous direct masked write that clobbered unrelated CRU bits during a
# lab probe must never appear in production code.
assert '0x00800080' not in text
assert '0x00800000' not in text

print("PASS: TSADC + failsafe watchdog recovery module architecture")
