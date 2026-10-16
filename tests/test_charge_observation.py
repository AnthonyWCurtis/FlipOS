"""Synthetic-only tests for the read-only FlipOS RK817 charging observer."""
from importlib.util import module_from_spec, spec_from_file_location
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parents[1]
SCRIPT = HERE / "tools/diagnostics/flip_charge_observe.py"
spec = spec_from_file_location("flip_charge_observe", SCRIPT)
observer = module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(observer)


class ChargeObservationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="charge-observe-fixture-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def write(self, rel, value):
        file = self.root / rel.lstrip("/")
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(value)
        return file

    def test_missing_sysfs_is_explicitly_unknown(self):
        record = observer.collect(self.root, 0)
        self.assertEqual(record["service_health"], "pid_file_unavailable")
        self.assertIsNone(record["temp_deci_c"])
        self.assertIsNone(record["battery_current_ua"])
        self.assertIsNone(record["requested_charger_limit_ma"])
        self.assertIsNone(record["battery_status"])

    def test_reads_integer_measurements_without_interpreting_charge_state(self):
        self.write("/sys/class/power_supply/battery/temp", "264\n")
        self.write("/sys/class/power_supply/battery/current_now", "-320000\n")
        self.write("/sys/class/power_supply/battery/voltage_now", "4078000\n")
        self.write("/sys/class/power_supply/battery/capacity", "63\n")
        self.write("/sys/class/power_supply/battery/status", "Charging\n")
        self.write("/sys/class/power_supply/usb/online", "1\n")
        self.write("/sys/class/power_supply/ac/online", "0\n")
        self.write("/sys/module/flip_rk817_charger/parameters/current_limit_ma", "500\n")
        record = observer.collect(self.root, 0)
        self.assertEqual(record["battery_current_ua"], -320000)
        self.assertEqual(record["temp_deci_c"], 264)
        self.assertEqual(record["battery_voltage_uv"], 4078000)
        self.assertEqual(record["battery_status"], "Charging")
        self.assertEqual(record["requested_charger_limit_ma"], 500)
        self.assertEqual(record["usb_online"], 1)
        self.assertEqual(record["ac_online"], 0)

    def test_invalid_values_rejected(self):
        self.write("/sys/class/power_supply/battery/temp", "5000\n")
        self.write("/sys/class/power_supply/battery/voltage_now", "secret\n")
        self.write("/sys/class/power_supply/usb/online", "2\n")
        self.write("/sys/class/power_supply/battery/status", "SECRET_STATUS\n")
        record = observer.collect(self.root, 0)
        for key in ("temp_deci_c", "battery_voltage_uv", "usb_online", "battery_status"):
            self.assertIsNone(record[key])

    def test_unrelated_privacy_sensitive_files_not_collected(self):
        self.write("/sys/class/power_supply/battery/temp", "238\n")
        self.write("/sys/class/power_supply/battery/serial_number", "PRIVATE_SERIAL\n")
        self.write("/tmp/flipos-dev-battery-temp-override", "SECRET_OVERRIDE\n")
        self.write("/mnt/SDCARD/flipos-dev/logs/charge.log", "PRIVATE_GAME_PATH\n")
        out = io.StringIO()
        observer.run(1, 0, self.root, out)
        text = out.getvalue()
        self.assertIn('"temp_deci_c":238', text)
        for secret in ("PRIVATE_SERIAL", "SECRET_OVERRIDE", "PRIVATE_GAME_PATH"):
            self.assertNotIn(secret, text)

    def test_pid_identity_confirmed_only_when_executable_inode_matches(self):
        exe = self.write(observer.EXPECTED_EXE, "mock executable")
        self.write(observer.PID_PATH, "7131\n")
        pointer = self.root / "proc/7131/exe"
        pointer.parent.mkdir(parents=True)
        pointer.symlink_to(exe)
        self.assertEqual(observer.service_health(self.root), "confirmed_process_identity")

    def test_pid_reuse_or_executable_replacement_is_not_confirmed(self):
        self.write(observer.EXPECTED_EXE, "new executable")
        different = self.write("/test/other-daemon", "old executable")
        self.write(observer.PID_PATH, "7131\n")
        pointer = self.root / "proc/7131/exe"
        pointer.parent.mkdir(parents=True)
        pointer.symlink_to(different)
        self.assertEqual(observer.service_health(self.root), "different_or_replaced_executable")

    def test_stale_pid_is_not_a_live_service(self):
        self.write(observer.PID_PATH, "7131\n")
        self.assertEqual(observer.service_health(self.root), "pid_not_running")

    def test_malformed_pid_never_influences_filesystem_path(self):
        for value in ("", "0\n", "1\n", "-3\n", "../secret\n", "999999999999\n", "2;rm\n"):
            with self.subTest(pid=value):
                self.write(observer.PID_PATH, value)
                self.assertEqual(observer.service_health(self.root), "invalid_pid_file")

    def test_rejects_fifo_without_hanging(self):
        path = self.root / "sys/class/power_supply/battery/temp"
        path.parent.mkdir(parents=True)
        os.mkfifo(path)
        self.assertIsNone(observer.collect(self.root, 0)["temp_deci_c"])

    def test_bounded_multi_sample_json_lines(self):
        self.write("/sys/class/power_supply/battery/temp", "250\n")
        out = io.StringIO()
        observer.run(3, 0, self.root, out)
        records = [json.loads(line) for line in out.getvalue().splitlines()]
        self.assertEqual(len(records), 3)
        self.assertTrue(all(record["source"] == "synthetic_fixture" for record in records))
        self.assertTrue(all(record["format"] == observer.VERSION for record in records))
        self.assertTrue(all(record["temp_deci_c"] == 250 for record in records))

    def test_live_fast_sampling_rejected(self):
        with self.assertRaises(ValueError):
            observer.validate(2, 1, None)
        with self.assertRaises(ValueError):
            observer.validate(121, 10, self.root)
        with self.assertRaises(ValueError):
            observer.validate(0, 10, self.root)
        with self.assertRaises(ValueError):
            observer.validate(2, 0, Path("relative"))

    def test_no_fixture_mutation(self):
        temp = self.write("/sys/class/power_supply/battery/temp", "250\n")
        pid = self.write(observer.PID_PATH, "9021\n")
        before = [(p.read_bytes(), p.stat().st_mtime_ns) for p in (temp, pid)]
        observer.run(2, 0, self.root, io.StringIO())
        self.assertEqual([(p.read_bytes(), p.stat().st_mtime_ns) for p in (temp, pid)], before)


if __name__ == "__main__":
    unittest.main()
