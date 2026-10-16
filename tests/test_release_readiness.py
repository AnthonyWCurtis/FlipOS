"""Host-only release evidence integrity tests; no real saves or hardware."""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'release'))
from release_readiness import GATES, GateError, checked_proof, evaluate, load_manifest, main

SHA = 'a' * 40
VERSION = '0.1.0-rc.1'


class EvidenceGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.files = {}
        for rel in ('artifacts/update.tar.gz', 'artifacts/install.zip'):
            self.files[rel] = self.proof(rel, b'fake fixture artifact')
        for gate in GATES:
            self.files[gate] = self.proof(f'evidence/{gate}.txt', f'{gate} fixture'.encode())
        self.manifest = {
            'format': 1,
            'version': VERSION,
            'source_sha': SHA,
            'artifacts': {'update': self.files['artifacts/update.tar.gz'], 'fresh_install': self.files['artifacts/install.zip']},
            'gates': {gate: {'status': 'passed', 'proofs': [self.files[gate]]} for gate in GATES},
        }

    def proof(self, rel, content):
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return {'path': rel, 'sha256': hashlib.sha256(content).hexdigest()}

    def check(self, data=None, version=VERSION, sha=SHA):
        return evaluate(data if data is not None else self.manifest, self.root, version, sha)

    def test_complete_synthetic_evidence_passes(self):
        result = self.check()
        self.assertTrue(result['ready'])
        self.assertEqual(len(result['passed']), len(GATES))

    def test_missing_one_gate_denies_release(self):
        data = copy.deepcopy(self.manifest)
        del data['gates'][GATES[-1]]
        self.assertFalse(self.check(data)['ready'])

    def test_pending_gate_denies_release(self):
        data = copy.deepcopy(self.manifest)
        data['gates'][GATES[0]] = {'status': 'pending', 'proofs': []}
        self.assertFalse(self.check(data)['ready'])

    def test_empty_proof_denies_release(self):
        data = copy.deepcopy(self.manifest)
        data['gates'][GATES[0]] = {'status': 'passed', 'proofs': []}
        self.assertFalse(self.check(data)['ready'])

    def test_wrong_commit_denies_release(self):
        self.assertFalse(self.check(sha='b' * 40)['ready'])

    def test_wrong_version_denies_release(self):
        self.assertFalse(self.check(version='0.1.0-rc.2')['ready'])

    def test_changed_artifact_denies_release(self):
        (self.root / 'artifacts/update.tar.gz').write_bytes(b'altered')
        self.assertFalse(self.check()['ready'])

    def test_changed_evidence_denies_release(self):
        (self.root / self.files[GATES[2]]['path']).write_bytes(b'altered')
        self.assertFalse(self.check()['ready'])

    def test_broken_symlink_denies_release(self):
        path = self.root / self.files[GATES[0]]['path']
        path.unlink()
        path.symlink_to('/etc/passwd')
        self.assertFalse(self.check()['ready'])

    def test_symlink_parent_denies_release(self):
        outside = self.root / 'outside'
        outside.mkdir()
        (outside / 'proof').write_bytes(b'example')
        (self.root / 'shortcut').symlink_to(outside, target_is_directory=True)
        with self.assertRaises(GateError):
            checked_proof({'path': 'shortcut/proof', 'sha256': hashlib.sha256(b'example').hexdigest()}, self.root)

    def test_traversal_denies_release(self):
        with self.assertRaises(GateError):
            checked_proof({'path': '../outside', 'sha256': 'a' * 64}, self.root)

    def test_fifo_denies_release(self):
        fifo = self.root / 'artifact_fifo'
        os.mkfifo(fifo)
        with self.assertRaises(GateError):
            checked_proof({'path': 'artifact_fifo', 'sha256': 'a' * 64}, self.root)

    def test_duplicate_manifest_keys_denied(self):
        f = self.root / 'manifest.json'
        f.write_text('{"format":1,"format":1}')
        with self.assertRaises(GateError):
            load_manifest(f)

    def test_manifest_schema_strict(self):
        f = self.root / 'manifest.json'
        f.write_text(json.dumps({**self.manifest, 'ignore': 'should reject'}))
        with self.assertRaises(GateError):
            load_manifest(f)

    def test_cli_pending_returns_nonzero(self):
        f = self.root / 'manifest.json'
        data = copy.deepcopy(self.manifest)
        data['gates'][GATES[5]] = {'status': 'pending', 'proofs': []}
        f.write_text(json.dumps(data))
        self.assertEqual(main(['--manifest', str(f), '--root', str(self.root), '--version', VERSION, '--source-sha', SHA]), 1)

    def test_cli_audit_never_blocks_ci(self):
        f = self.root / 'manifest.json'
        data = copy.deepcopy(self.manifest)
        data['gates'][GATES[5]] = {'status': 'pending', 'proofs': []}
        f.write_text(json.dumps(data))
        self.assertEqual(main(['--manifest', str(f), '--root', str(self.root), '--version', VERSION, '--source-sha', SHA, '--audit']), 0)

    def test_truncated_missing_manifest_blocks(self):
        self.assertEqual(main(['--manifest', str(self.root / 'absent.json'), '--root', str(self.root), '--version', VERSION, '--source-sha', SHA]), 1)


if __name__ == '__main__':
    unittest.main()