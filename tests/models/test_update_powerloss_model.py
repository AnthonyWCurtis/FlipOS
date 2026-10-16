"""Host-only crash-state exploration for the proposed update journal protocol.

These tests exercise model invariants, NOT the production installer/boot hook.
"""

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from model_update_recovery import RecoveryError, UpdateRecoveryModel


class UpdatePowerlossTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='flipos-powerloss-model-')
        self.addCleanup(self.temp.cleanup)
        self.model = UpdateRecoveryModel(Path(self.temp.name))
        self.model.seed('bin/flip-frontend', b'old-frontend')
        self.model.seed('bin/flip-gb', b'old-gb')
        self.update = {
            'bin/flip-frontend': b'new-frontend',
            'bin/flip-gb': b'new-gb',
            'VERSION': b'new-version',
        }

    def assert_old(self):
        self.assertEqual((self.model.active / 'bin/flip-frontend').read_bytes(), b'old-frontend')
        self.assertEqual((self.model.active / 'bin/flip-gb').read_bytes(), b'old-gb')
        self.assertFalse((self.model.active / 'VERSION').exists())

    def test_pending_at_each_crash_point_recovers(self):
        keys = list(self.update)
        for count in range(0, len(keys) + 1):
            with self.subTest(installed=count):
                with tempfile.TemporaryDirectory() as root:
                    model = UpdateRecoveryModel(Path(root))
                    model.seed('bin/flip-frontend', b'old-frontend')
                    model.seed('bin/flip-gb', b'old-gb')
                    model.prepare(self.update)
                    for name in keys[:count]:
                        model.apply_one(name)
                    self.assertEqual(model.boot_reconcile(), 'rolled-back')
                    self.assertEqual((model.active / 'bin/flip-frontend').read_bytes(), b'old-frontend')
                    self.assertEqual((model.active / 'bin/flip-gb').read_bytes(), b'old-gb')
                    self.assertFalse((model.active / 'VERSION').exists())
                    self.assertEqual(model.boot_reconcile(), 'clean')

    def test_all_new_is_not_committed_until_marker(self):
        self.model.prepare(self.update)
        for name in self.update:
            self.model.apply_one(name)
        self.assertEqual(self.model.boot_reconcile(), 'rolled-back')
        self.assert_old()

    def test_committed_allows_valid_new_generation(self):
        self.model.prepare(self.update)
        for name in self.update:
            self.model.apply_one(name)
        self.model.commit()
        self.assertEqual(self.model.boot_reconcile(), 'use-new')
        self.assertEqual((self.model.active / 'bin/flip-gb').read_bytes(), b'new-gb')

    def test_commit_rejects_partial_generation(self):
        self.model.prepare(self.update)
        self.model.apply_one('bin/flip-gb')
        with self.assertRaises(RecoveryError):
            self.model.commit()
        self.assertEqual(self.model.boot_reconcile(), 'rolled-back')
        self.assert_old()

    def test_corrupt_backup_blocks_before_modifying_any_file(self):
        self.model.prepare(self.update)
        self.model.apply_one('bin/flip-frontend')
        before = (self.model.active / 'bin/flip-frontend').read_bytes()
        (self.model.tx / 'backup/bin/flip-gb').write_bytes(b'broken')
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile()
        self.assertEqual((self.model.active / 'bin/flip-frontend').read_bytes(), before)
        self.assertTrue(self.model.marker.exists())

    def test_missing_backup_blocks_and_preserves_marker(self):
        self.model.prepare(self.update)
        self.model.apply_one('bin/flip-frontend')
        (self.model.tx / 'backup/bin/flip-gb').unlink()
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile()
        self.assertTrue(self.model.marker.exists())

    def test_readonly_pending_fails_closed(self):
        self.model.prepare(self.update)
        self.model.apply_one('bin/flip-frontend')
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile(writable=False)
        self.assertEqual((self.model.active / 'bin/flip-frontend').read_bytes(), b'new-frontend')
        self.assertTrue(self.model.marker.exists())

    def test_committed_corruption_fails_closed(self):
        self.model.prepare(self.update)
        for name in self.update:
            self.model.apply_one(name)
        self.model.commit()
        (self.model.active / 'bin/flip-gb').write_bytes(b'bad')
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile()
        self.assertTrue(self.model.marker.exists())

    def test_invalid_marker_is_not_interpreted_as_clean(self):
        self.model.prepare(self.update)
        self.model.marker.write_text('not-json')
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile()
        self.assertTrue(self.model.marker.exists())

    def test_traversal_marker_is_rejected(self):
        self.model.prepare(self.update)
        journal = json.loads(self.model.marker.read_text())
        journal['files'][0]['name'] = '../outside'
        self.model.marker.write_text(json.dumps(journal))
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile()

    def test_stage_tamper_rejected(self):
        self.model.prepare(self.update)
        (self.model.tx / 'incoming/bin/flip-frontend').write_bytes(b'evil')
        with self.assertRaises(RecoveryError):
            self.model.apply_one('bin/flip-frontend')
        self.assert_old()

    def test_journal_must_exist_if_transaction_folder_exists(self):
        self.model.tx.mkdir()
        with self.assertRaises(RecoveryError):
            self.model.boot_reconcile()

    def test_new_prepare_does_not_overwrite_unreconciled_journal(self):
        self.model.prepare(self.update)
        with self.assertRaises(RecoveryError):
            self.model.prepare(self.update)


if __name__ == '__main__':
    unittest.main()
