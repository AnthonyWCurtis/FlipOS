"""Disposable host tests for the consent-only ADB charging report collector."""
from importlib.util import module_from_spec, spec_from_file_location
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[1] / "tools/device/flip-charge-collect-adb.py"
spec = spec_from_file_location("flip_charge_collect_adb", SOURCE)
collector = module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(collector)


class ChargeADBCollectorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="flipos-adb-charge-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / "private-report.json"

    def test_requires_explicit_consent_before_invoking_adb(self):
        with patch.object(collector.subprocess, "run") as run:
            with self.assertRaises(SystemExit) as caught:
                collector.main(["--output", str(self.path)])
            self.assertEqual(caught.exception.code, 2)
            run.assert_not_called()
        self.assertFalse(self.path.exists())

    def test_device_selection_requires_one_authorised_device(self):
        with patch.object(collector, "call", return_value="List of devices attached\n"):
            with self.assertRaises(collector.CollectionError):
                collector.choose_device("adb", None)
        with patch.object(collector, "call", return_value="A\tdevice\nB\tdevice\n"):
            with self.assertRaises(collector.CollectionError):
                collector.choose_device("adb", None)
            self.assertEqual(collector.choose_device("adb", "A"), "A")
            with self.assertRaises(collector.CollectionError):
                collector.choose_device("adb", "C")

    def test_rejects_disconnected_device_state(self):
        with patch.object(collector, "call", return_value="A\tunauthorized\nB\toffline\n"):
            with self.assertRaises(collector.CollectionError):
                collector.choose_device("adb", None)

    def test_strict_allowlist_discards_sensitive_and_invalid_fields(self):
        raw = ("temp_deci_c=238\ncurrent_ua=-15000\nusb_online=1\n"
               "battery_status=Charging\n"
               "rom=/secret/Pokemon.gba\nssid=my-home\n"
               "temp_deci_c=999\nrequested_cap_ma=500\n")
        parsed = collector.parse(raw)
        self.assertEqual(parsed["temp_deci_c"], 238)
        self.assertEqual(parsed["usb_online"], 1)
        self.assertEqual(parsed["requested_cap_ma"], 500)
        self.assertEqual(parsed["battery_status"], "Charging")
        self.assertEqual(parsed["current_ua"], -15000)
        self.assertNotIn("rom", parsed)
        self.assertNotIn("ssid", parsed)

    def test_out_of_range_and_unrecognized_status_are_null(self):
        parsed = collector.parse("temp_deci_c=9999\npercent=102\n"
                                 "requested_cap_ma=9999\nbattery_status=SECRET\n"
                                 "kernel_release=exploited spaces\n"
                                 "module_file_sha256=bad\n")
        for key in ("temp_deci_c", "percent", "requested_cap_ma",
                    "battery_status", "kernel_release", "module_file_sha256"):
            self.assertIsNone(parsed[key])

    def test_remote_program_has_no_mutating_device_commands(self):
        remote = collector.REMOTE_SH
        for unsafe in ("insmod", "rmmod", "i2cset", "i2cget", "chmod",
                       "chown", "reboot", "rm -", "kill ", "dd if=", "tee "):
            self.assertNotIn(unsafe, remote)
        self.assertNotIn(">>", remote)
        self.assertIn("sha256sum", remote)
        self.assertIn("current_now", remote)

    def test_one_sample_uses_fixed_stdin_shell_and_no_device_copy(self):
        calls = []
        def fake_run(cmd, **kwargs):
            calls.append((cmd, kwargs.get("input")))
            output = ("List of devices attached\nSERIAL123\tdevice\n"
                      if cmd == ["adb", "devices"] else
                      "temp_deci_c=251\nmodule_loaded=1\nbattery_status=Charging\n")
            return subprocess.CompletedProcess(cmd, 0, output, "")
        with patch.object(collector.subprocess, "run", side_effect=fake_run):
            entries = collector.collect("adb", 1, 0, None)
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0]["temp_deci_c"], 251)
        self.assertEqual(calls[1][0],
                         ["adb", "-s", "SERIAL123", "shell", "sh", "-s"])
        self.assertEqual(calls[1][1], collector.REMOTE_SH)
        self.assertFalse(any("push" in cmd or "pull" in cmd for cmd, _ in calls))

    def test_failed_read_does_not_create_report(self):
        def fake_run(cmd, **kwargs):
            if cmd == ["adb", "devices"]:
                return subprocess.CompletedProcess(cmd, 0, "S\tdevice\n", "")
            return subprocess.CompletedProcess(cmd, 1, "", "device offline")
        with patch.object(collector.subprocess, "run", side_effect=fake_run):
            with self.assertRaises(collector.CollectionError):
                collector.collect("adb", 1, 0, None)
        self.assertFalse(self.path.exists())

    def test_sampling_bounds_fail_before_device_command(self):
        with patch.object(collector, "choose_device") as chooser:
            for count, interval in ((0, 10), (31, 10), (3, 0), (2, 4), (1, -1)):
                with self.subTest(count=count, interval=interval):
                    with self.assertRaises(ValueError):
                        collector.collect("adb", count, interval, None)
            chooser.assert_not_called()

    def test_multiple_samples_are_bounded_and_timed(self):
        with patch.object(collector, "choose_device", return_value="S"), \
             patch.object(collector, "call", return_value="temp_deci_c=250\n"), \
             patch.object(collector.time, "sleep") as sleep:
            entries = collector.collect("adb", 3, 5, None)
        self.assertEqual(len(entries), 3)
        self.assertEqual([x["sample"] for x in entries], [1, 2, 3])
        self.assertEqual(sleep.call_count, 2)
        sleep.assert_called_with(5)

    def test_output_is_private_new_file_and_never_overwrites(self):
        collector.write_report(self.path, [{"temp_deci_c": 250}])
        data = json.loads(self.path.read_text())
        self.assertFalse(data["uploaded"])
        self.assertFalse(data["device_writes"])
        self.assertFalse(data["thermal_cutoff_hardware_verified"])
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        original = self.path.read_bytes()
        with self.assertRaises(FileExistsError):
            collector.write_report(self.path, [{"temp_deci_c": 300}])
        self.assertEqual(self.path.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
