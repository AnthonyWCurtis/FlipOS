from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
module = (ROOT / "tools/kernel/flip_rk817_charger/flip_rk817_charger.c").read_text()
charger = (ROOT / "src/runtime/flip_charger.c").read_text()

assert 'RK817_CHRG_OUT_REG 0xE4' in module
assert 'RK817_CHRG_CUR_MASK 0x07' in module
assert 'i2c_smbus_write_byte_data(' in module
assert 'i2c_smbus_read_byte_data(' in module
assert 'stock_selector = value & RK817_CHRG_CUR_MASK' in module
assert 'apply_current_limit(-1)' in module
assert 'CHRG_EN' not in module
assert 'POWERPATH' not in module
assert '0xE6' not in module
assert 'max_chrg_voltage' not in module
assert 'flip_charge_policy_evaluate(' in charger
assert 'target_ma = 500' in charger
assert 'write_current(-1)' in charger

print("RK817 charge-current helper architecture passed")
assert 'override_active' in module
assert 'target_ma = baseline_ma < ma ? baseline_ma : ma' in module
assert 'if (!override_active)' in module
