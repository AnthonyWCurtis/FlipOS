#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c"

text = SRC.read_text(encoding="utf-8")

required = [
    '#define TSADC_INT_EN 0x08',
    'SHUT_GPIO_BITS',
    'SHUT_CRU_BITS',
    '#define CRU_GLB_RST_CON 0x00dc',
    'CRU_WDT_RESET_EXT BIT(7)',
    '"rockchip,rk3568-cru"',
    '"rockchip,rk3568-tsadc"',
    '"gpio"',
    '"otpout"',
    'module_param_cb(recovery',
    'if (armed)\n        set_route(false);',
    'restore_cru_wdt_reset_extension()',
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
]:
    assert forbidden not in text, f"forbidden recovery behavior present: {forbidden}"

assert 'val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);' in text
assert 'val |= original_route_bits;' in text

assert 'val = readl_relaxed(cru_regs + CRU_GLB_RST_CON);' in text
assert 'val |= CRU_WDT_RESET_EXT;' in text
assert 'val &= ~CRU_WDT_RESET_EXT;' in text
assert 'val |= original_cru_wdt_reset_ext;' in text

print("PASS: TSADC + extended watchdog reset recovery architecture")
