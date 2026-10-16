#!/usr/bin/env python3
"""No-hardware tests of the opt-in USB mode controller."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools/device/flip-usb-mode"


class USBModeTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix="flipos-usb-mode-")
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.gadget = self.root / "sys/kernel/config/usb_gadget/rockchip"
        self.config = self.gadget / "configs/b.1"
        self.adb = self.config / "f-ffs.adb"
        self.mtp = self.config / "f-ffs.mtp"
        self.udc = self.gadget / "UDC"
        self.android = self.root / "sys/class/android_usb/android0/state"
        self.helper = self.root / "mnt/SDCARD/flipos-dev/bin/flip-usb-mtp"
        self.owner = self.root / "tmp/flipos-usb-owned"
        self.config.mkdir(parents=True)
        (self.gadget / "functions/ffs.adb").mkdir(parents=True)
        (self.gadget / "functions/ffs.mtp").mkdir(parents=True)
        self.adb.symlink_to(self.gadget / "functions/ffs.adb", target_is_directory=True)
        self.udc.write_text("fcc00000.dwc3\n")
        self.android.parent.mkdir(parents=True)
        self.android.write_text("CONFIGURED\n")
        self.helper.parent.mkdir(parents=True)
        (self.root / "tmp").mkdir()
        self.helper.write_text(
            '#!/bin/sh\n'
            'set -eu\n'
            'ln -s "$FLIP_USB_TEST_ROOT/sys/kernel/config/usb_gadget/rockchip/functions/ffs.mtp" '
            '"$FLIP_USB_TEST_ROOT/sys/kernel/config/usb_gadget/rockchip/configs/b.1/f-ffs.mtp"\n'
            'touch "$FLIP_USB_TEST_ROOT/tmp/flipos-usb-owned"\n'
        )
        self.helper.chmod(0o755)

    def call(self, command, *, root=None):
        env = dict(os.environ, FLIP_USB_TEST_ROOT=str(root or self.root))
        return subprocess.run(
            ["sh", str(SCRIPT), command], text=True,
            capture_output=True, check=False, timeout=6, env=env,
        )

    def test_stock_adb_status(self):
        self.assertEqual(self.call("status").stdout.strip(), "ADB_ONLY")

    def test_boot_never_changes_usb_or_starts_mtp(self):
        before = self.udc.read_bytes()
        self.assertEqual(self.call("boot").returncode, 0)
        self.assertEqual(self.udc.read_bytes(), before)
        self.assertFalse(self.mtp.is_symlink())
        self.assertFalse(self.owner.exists())

    def test_unbound_boot_fails_without_changing_gadget(self):
        self.udc.write_text("")
        result = self.call("boot")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.udc.read_text(), "")
        self.assertFalse(self.mtp.exists())

    def test_explicit_mtp_transition_and_idempotence(self):
        result = self.call("mtp")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.mtp.is_symlink())
        self.assertEqual(self.call("status").stdout.strip(), "ADB_MTP")
        second = self.call("mtp")
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertTrue(self.adb.is_symlink())

    def test_mtp_requires_usb_configured(self):
        self.android.write_text("DISCONNECTED\n")
        response = self.call("mtp")
        self.assertNotEqual(response.returncode, 0)
        self.assertFalse(self.mtp.is_symlink())

    def test_mtp_refused_when_updater_or_game_busy(self):
        (self.root / "busy-runtime").touch()
        response = self.call("mtp")
        self.assertNotEqual(response.returncode, 0)
        self.assertIn("active_game_or_updater", response.stderr)
        self.assertFalse(self.mtp.is_symlink())

    def test_return_to_adb_removes_only_mtp_link(self):
        self.assertEqual(self.call("mtp").returncode, 0)
        other = self.config / "f-unrelated"
        other.symlink_to(self.gadget / "functions/ffs.adb")
        result = self.call("adb")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.call("status").stdout.strip(), "ADB_ONLY")
        self.assertTrue(self.adb.is_symlink())
        self.assertTrue(other.is_symlink())
        self.assertFalse(self.mtp.is_symlink())
        self.assertEqual(self.udc.read_text().strip(), "fcc00000.dwc3")

    def test_unknown_usb_function_prevents_a_mode_change(self):
        extra = self.config / "f-rndis.net"
        extra.symlink_to(self.gadget / "functions/ffs.adb")
        self.assertEqual(self.call("status").stdout.strip(), "UNKNOWN")
        result = self.call("mtp")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(extra.is_symlink())
        self.assertTrue(self.adb.is_symlink())
        self.assertFalse(self.mtp.is_symlink())
        self.assertEqual(self.udc.read_text().strip(), "fcc00000.dwc3")

    def test_return_to_adb_refuses_unknown_owner(self):
        self.assertEqual(self.call("mtp").returncode, 0)
        self.owner.unlink()
        result = self.call("adb")
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.mtp.is_symlink())
        self.assertNotEqual(self.udc.read_text().strip(), "")

    def test_refuse_unbound_or_unknown_gadget(self):
        self.udc.write_text("")
        self.assertNotEqual(self.call("mtp").returncode, 0)
        self.assertNotEqual(self.call("adb").returncode, 0)
        self.assertEqual(self.udc.read_text(), "")

    def test_locked_transition_does_not_change_config(self):
        (self.root / "tmp/flipos-usb-mode.lock").mkdir()
        result = self.call("mtp")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.mtp.is_symlink())

    def test_unknown_command_is_rejected(self):
        result = self.call("factory-reset")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(self.call("status").stdout.strip(), "ADB_ONLY")

    def test_status_does_not_write_files(self):
        before = sorted(str(p.relative_to(self.root)) for p in self.root.rglob("*"))
        self.call("status")
        after = sorted(str(p.relative_to(self.root)) for p in self.root.rglob("*"))
        self.assertEqual(before, after)

    def test_mode_never_writes_on_real_paths_under_fixture(self):
        self.assertIn("FLIP_USB_TEST_ROOT", SCRIPT.read_text())
        self.assertNotIn('rm -f "$CONFIG"/f-*', SCRIPT.read_text())


if __name__ == "__main__":
    unittest.main()
