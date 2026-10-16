"""Safe host-only log-export tests; no live device or network access."""
from importlib.util import module_from_spec, spec_from_file_location
import json
import os
from pathlib import Path
import stat
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / 'tools/diagnostics/flip_diagnostics_export.py'
spec = spec_from_file_location('flip_diagnostics_export', SCRIPT)
module = module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)


class DiagnosticsExportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.src = self.root / 'frontend.log'
        self.dst = self.root / 'report.json'

    def write(self, text):
        self.src.write_text(text, encoding='utf-8')

    def export_json(self):
        module.export([self.src], self.dst, consent=True)
        return json.loads(self.dst.read_text(encoding='utf-8'))

    def test_requires_explicit_consent(self):
        self.write('FLIP_CORE_HOST_FPS 59.7 target=59.7\n')
        with self.assertRaises(PermissionError):
            module.export([self.src], self.dst)
        self.assertFalse(self.dst.exists())

    def test_numeric_metrics_only(self):
        self.write('FLIP_PERF: fps=59.723 frames=60 late_max_us=41 resyncs=0 rom=/Roms/secret.gb\n')
        report = self.export_json()
        metrics = report['sources'][0]['events'][0]['metrics']
        self.assertEqual(metrics, {'fps': 59.723, 'frames': 60, 'late_max_us': 41, 'resyncs': 0})
        self.assertNotIn('rom', self.dst.read_text())

    def test_fps_special_prefix(self):
        self.write('FLIP_CORE_HOST_FPS 59.728 target=59.728\n')
        event = self.export_json()['sources'][0]['events'][0]
        self.assertEqual(event, {'kind': 'FLIP_CORE_HOST_FPS', 'metrics': {'fps': 59.728, 'target': 59.728}})

    def test_audio_gpu_and_clock_metrics(self):
        self.write('FLIP_AUDIO_PERF: queue_dropped_frames=20 path=/Roms/foo.gb\n'
                   'FLIP_GPU_SERVICE_PERF: invalid=3 render_avg_us=501\n'
                   'FLIP_CPU_DYNAMIC: raised max_khz=816000->1104000 fps=58.5 target=59.7\n')
        kinds = [e['kind'] for e in self.export_json()['sources'][0]['events']]
        self.assertEqual(kinds, ['FLIP_AUDIO_PERF', 'FLIP_GPU_SERVICE_PERF', 'FLIP_CPU_DYNAMIC'])

    def test_arbitrary_text_and_credentials_not_copied(self):
        sensitive = ('SSID=home-wifi password=rainbow123 token=deadbeef '
                     'email=user@example.org 192.168.1.42 '
                     'rom=/mnt/SDCARD/Roms/GBA/secret.gba')
        self.write('FLIP_SAVE: SAVE_FAILED stage=write ' + sensitive + '\n'
                   'FLIP_PERF: fps=59.7 ' + sensitive + '\n')
        report = self.export_json()
        output = self.dst.read_text()
        self.assertEqual(report['sources'][0]['alert_lines_count'], 1)
        for secret in ('rainbow123', 'deadbeef', 'home-wifi', 'user@example.org', '192.168.1.42', 'secret.gba'):
            self.assertNotIn(secret, output)

    def test_disallowed_log_filename_rejected(self):
        path = self.root / 'wifi.conf'
        path.write_text('password=hi')
        with self.assertRaises(ValueError):
            module.export([path], self.dst, consent=True)

    def test_symlink_rejected(self):
        target = self.root / 'secret'
        target.write_text('token=private')
        self.src.symlink_to(target)
        with self.assertRaises(OSError):
            module.export([self.src], self.dst, consent=True)

    def test_fifo_rejected_without_blocking(self):
        os.mkfifo(self.src)
        with self.assertRaises(ValueError):
            module.export([self.src], self.dst, consent=True)

    def test_existing_destination_untouched(self):
        self.write('FLIP_PERF: fps=59.7\n')
        self.dst.write_text('keep me')
        with self.assertRaises(FileExistsError):
            module.export([self.src], self.dst, consent=True)
        self.assertEqual(self.dst.read_text(), 'keep me')

    def test_new_report_private_permissions(self):
        self.write('FLIP_PERF: fps=59.7\n')
        self.export_json()
        self.assertEqual(stat.S_IMODE(self.dst.stat().st_mode), 0o600)

    def test_invalid_numeric_values_rejected(self):
        self.write('FLIP_PERF: fps=nan frames=inf late_max_us=9999999999999999999999 resyncs=2\n')
        report = self.export_json()
        self.assertEqual(report['sources'][0]['events'][0]['metrics'], {'resyncs': 2})

    def test_alerts_only_as_counts(self):
        self.write('FLIP_GPU_SERVICE: render FAILED rom=/secret/name.gb\n'
                   'FLIP_CORE_HOST: state ERROR key=private\n')
        report = self.export_json()
        self.assertEqual(report['sources'][0]['alert_lines_count'], 2)
        self.assertEqual(report['sources'][0]['events'], [])

    def test_max_events_bounded(self):
        self.write('FLIP_PERF: fps=59.7\n' * 500)
        report = self.export_json()
        self.assertEqual(len(report['sources'][0]['events']), module.MAX_EVENTS_PER_SOURCE)

    def test_tail_truncation(self):
        self.write('x' * (module.MAX_TAIL_BYTES + 10) + '\nFLIP_PERF: fps=55.5\n')
        report = self.export_json()
        self.assertTrue(report['sources'][0]['tail_truncated'])
        self.assertEqual(len(report['sources'][0]['events']), 1)

    def test_overlarge_source_rejected(self):
        self.src.write_bytes(b'x')
        with self.src.open('r+b') as f:
            f.truncate(module.MAX_SOURCE_SIZE + 1)
        with self.assertRaises(ValueError):
            module.export([self.src], self.dst, consent=True)

    def test_duplicate_source_rejected(self):
        self.write('FLIP_PERF: fps=55\n')
        with self.assertRaises(ValueError):
            module.export([self.src, self.src], self.dst, consent=True)


if __name__ == '__main__':
    unittest.main()
