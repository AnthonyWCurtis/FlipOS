"""Host-only tests for the manual RK817 read-only inspection script.

All device paths are remapped into TemporaryDirectory fixtures; no real
power_supply sysfs, PMIC, charger modules or physical device are accessed.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/device/flip-charge-readonly-inventory.sh"


class ChargeReadOnlyInventoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="flipos-charge-inventory-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def write(self, relative, value):
        path = self.root / relative.lstrip("/")
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(value, str):
            path.write_text(value)
        else:
            path.write_bytes(value)
        return path

    def run_probe(self, root=None):
        env = os.environ.copy()
        env["FLIP_CHARGE_PROBE_ROOT"] = str(root if root is not None else self.root)
        return subprocess.run(
            ["sh", str(SCRIPT)], capture_output=True, text=True, env=env,
            timeout=5, check=False,
        )

    def test_reads_allowlisted_fields_without_writes(self):
        temp = self.write("/sys/class/power_supply/battery/temp", "253\n")
        cap = self.write("/sys/module/flip_rk817_charger/parameters/current_limit_ma", "1000\n")
        before = [(f.read_bytes(), f.stat().st_mtime_ns) for f in (temp, cap)]
        result = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("source=synthetic_fixture", result.stdout)
        self.assertIn("battery_temp_deci_c=253", result.stdout)
        self.assertIn("module_limit_ma=1000", result.stdout)
        self.assertIn("register_writes=none", result.stdout)
        self.assertIn("network_uploads=none", result.stdout)
        self.assertEqual(before, [(f.read_bytes(), f.stat().st_mtime_ns) for f in (temp, cap)])
        self.assertFalse((self.root / "charge.log").exists())

    def test_missing_sources_report_unavailable(self):
        result = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stderr)
        for key in ("battery_temp_deci_c", "usb_online", "sd_module_sha256"):
            self.assertIn(key + "=unavailable", result.stdout)

    def test_module_digest_is_only_reported_when_a_file_is_present(self):
        contents = b"synthetic test module, not a real kernel module"
        self.write("/mnt/SDCARD/flipos-dev/lib/flip_rk817_charger.ko", contents)
        result = self.run_probe()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("sd_module_sha256=" + hashlib.sha256(contents).hexdigest(), result.stdout)
        self.assertNotIn("synthetic test module", result.stdout)

    def test_omits_sensitive_and_non_allowlisted_files(self):
        self.write("/sys/class/power_supply/battery/temp", "250\n")
        self.write("/sys/class/power_supply/battery/serial_number", "PRIVATE-SERIAL-NUMBER\n")
        self.write("/tmp/flipos-dev-battery-temp-override", "SECRET-OVERRIDE\n")
        self.write("/mnt/SDCARD/flipos-dev/logs/charge.log", "PRIVATE-LOG-TOKEN\n")
        result = self.run_probe()
        self.assertEqual(result.returncode, 0)
        self.assertIn("battery_temp_deci_c=250", result.stdout)
        for fragment in ("PRIVATE-SERIAL-NUMBER", "SECRET-OVERRIDE", "PRIVATE-LOG-TOKEN"):
            self.assertNotIn(fragment, result.stdout)

    def test_rejects_relative_fixture_roots(self):
        result = self.run_probe(root="relative-path")
        self.assertEqual(result.returncode, 2)
        self.assertNotIn("FLIP_CHARGE_READONLY_INVENTORY_V1", result.stdout)

    def test_bounds_and_sanitizes_printed_values(self):
        self.write("/sys/class/power_supply/battery/status", ("A" * 1000) + "\nSECOND LINE PRIVATE\n")
        result = self.run_probe()
        self.assertEqual(result.returncode, 0)
        self.assertIn("battery_status=" + "A" * 80, result.stdout)
        self.assertNotIn("SECOND LINE PRIVATE", result.stdout)
        self.assertNotIn("A" * 81, result.stdout)

    def test_reports_driver_presence_without_recursive_discovery(self):
        (self.root / "sys/bus/platform/drivers/rk817-charger").mkdir(parents=True)
        self.write("/proc/modules", "flip_rk817_charger 16384 0 - Live 0x00000000\n"
                                  "unrelated_driver 9999 0 - Live 0x00000000\n")
        result = self.run_probe()
        self.assertEqual(result.returncode, 0)
        self.assertIn("platform_driver_rk817-charger=present", result.stdout)
        self.assertIn("loaded_module_flip_rk817_charger=yes", result.stdout)
        self.assertNotIn("unrelated_driver", result.stdout)


if __name__ == "__main__":
    unittest.main()
