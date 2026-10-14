#!/usr/bin/env python3
"""Pure-Python fixtures for release input integrity and ELF64 module vetting."""

import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AUDIT = ROOT / 'tools/kernel/verify_release_module_inputs.py'
NAMES = ('flip_opp_voltage.ko', 'flip_opp_runtime_profile.ko',
         'flip_tsadc_recovery.ko', 'flip_rk817_charger.ko')


def align(value, n=8):
    return (value + n - 1) // n * n


def make_module(name, vermagic='5.10.160 SMP mod_unload aarch64', machine=183,
                license_name='GPL'):
    """Generate a minimal little-endian AArch64 ELF relocatable, not executable."""
    names = b'\0.shstrtab\0.modinfo\0'
    info = (f'name={name}\0vermagic={vermagic}\0license={license_name}\0depends=\0').encode()
    names_offset = 64
    info_offset = align(names_offset + len(names))
    shoff = align(info_offset + len(info))
    out = bytearray(shoff + 3 * 64)
    out[:16] = b'\x7fELF\x02\x01\x01' + b'\0' * 9
    struct.pack_into('<HHIQQQIHHHHHH', out, 16,
                     1, machine, 1, 0, 0, shoff, 0, 64, 0, 0, 64, 3, 1)
    out[names_offset:names_offset+len(names)] = names
    out[info_offset:info_offset+len(info)] = info
    struct.pack_into('<IIQQQQIIQQ', out, shoff + 64,
                     1, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)
    struct.pack_into('<IIQQQQIIQQ', out, shoff + 128,
                     names.index(b'.modinfo'), 1, 0, 0, info_offset, len(info), 0, 0, 1, 0)
    return bytes(out)


class ReleaseModuleInputTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='flip-module-test-')
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)
        self.moddir = self.dir / 'modules'
        self.moddir.mkdir()
        self.archive = self.dir / 'download.tar.gz'
        self.archive.write_bytes(b'not-extracted-until-checksum')
        self.lock = {'format': 1, 'expected_kernel_base': '5.10.160',
                     'expected_vermagic': '5.10.160 SMP mod_unload aarch64',
                     'archive_sha256': hashlib.sha256(self.archive.read_bytes()).hexdigest(),
                     'modules': {}}
        for name in NAMES:
            data = make_module(name[:-3])
            (self.moddir / name).write_bytes(data)
            self.lock['modules'][name] = hashlib.sha256(data).hexdigest()
        self.lockpath = self.dir / 'lock.json'
        self.store_lock()

    def store_lock(self):
        self.lockpath.write_text(json.dumps(self.lock))

    def run_audit(self):
        return subprocess.run([sys.executable, str(AUDIT), '--lock', str(self.lockpath),
                               '--archive', str(self.archive), '--modules', str(self.moddir)],
                              capture_output=True, text=True)

    def replace_module(self, name, data, update_lock=True):
        (self.moddir / name).write_bytes(data)
        if update_lock:
            self.lock['modules'][name] = hashlib.sha256(data).hexdigest()
            self.store_lock()

    def test_valid_module_set(self):
        result = self.run_audit()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count('MODULE_ABI_PRECHECK_OK'), 4)
        self.assertIn('MODULE_VERMAGIC_CONSISTENT', result.stdout)

    def test_reject_archive_digest(self):
        self.archive.write_bytes(b'changed')
        self.assertIn('SHA-256 mismatch', self.run_audit().stderr)

    def test_reject_changed_module_digest(self):
        (self.moddir / NAMES[0]).write_bytes(b'changed')
        self.assertIn('SHA-256 mismatch', self.run_audit().stderr)

    def test_reject_wrong_machine_even_with_pinned_hash(self):
        self.replace_module(NAMES[0], make_module(NAMES[0][:-3], machine=62))
        self.assertIn('AArch64 relocatable ELF', self.run_audit().stderr)

    def test_reject_wrong_module_name(self):
        self.replace_module(NAMES[0], make_module('evil'))
        self.assertIn('module name mismatch', self.run_audit().stderr)

    def test_reject_wrong_kernel_release(self):
        self.replace_module(NAMES[0], make_module(NAMES[0][:-3], vermagic='7.0.2 SMP aarch64'))
        self.assertIn('unexpected kernel version', self.run_audit().stderr)

    def test_reject_inconsistent_vermagic(self):
        self.replace_module(NAMES[0], make_module(NAMES[0][:-3], vermagic='5.10.160 SMP preempt mod_unload aarch64'))
        self.assertIn('unexpected module vermagic', self.run_audit().stderr)

    def test_reject_malformed_section_offsets(self):
        data = bytearray(make_module(NAMES[0][:-3]))
        struct.pack_into('<Q', data, 40, 1 << 60)
        self.replace_module(NAMES[0], bytes(data))
        self.assertIn('out-of-range ELF section table', self.run_audit().stderr)

    def test_reject_disallowed_license(self):
        self.replace_module(NAMES[0], make_module(NAMES[0][:-3], license_name='Proprietary'))
        self.assertIn('unexpected module license', self.run_audit().stderr)

    def test_reject_missing_module(self):
        (self.moddir / NAMES[0]).unlink()
        self.assertIn('missing/unsafe regular file', self.run_audit().stderr)

    def test_reject_malformed_lock(self):
        self.lock['modules'].pop(NAMES[0])
        self.store_lock()
        self.assertIn('must contain four modules', self.run_audit().stderr)


if __name__ == '__main__':
    unittest.main()
