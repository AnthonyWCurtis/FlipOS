#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "tools/kernel/flip_tsadc_recovery/flip_tsadc_recovery.c"

text = SRC.read_text(encoding="utf-8")

required = [
    '#define TSADC_INT_EN 0x08',
    'SHUT_GPIO_BITS',
    'SHUT_CRU_BITS',
    'pinctrl_lookup_state(tsadc_pinctrl, "gpio")',
    'pinctrl_lookup_state(tsadc_pinctrl, "otpout")',
    'module_param_cb(recovery',
    'if (armed)\n        set_route(false);',
]
for needle in required:
    assert needle in text, f"missing required safety behavior: {needle}"

# This module is deliberately route-only. It must not touch the thermal
# shutdown threshold/comparator registers or invent a lower trip point.
for forbidden in [
    'COMP_SHUT',
    'hw-tshut-temp',
    '120000',
    '115000',
    '95000',
    'set_tshut_temp',
]:
    assert forbidden not in text, f"route helper must not alter thermal threshold: {forbidden}"

# Restore must preserve unrelated INT_EN bits and only replace route bits.
assert 'val &= ~(SHUT_GPIO_BITS | SHUT_CRU_BITS);' in text
assert 'val |= original_route_bits;' in text

print("PASS: TSADC recovery module architecture")
