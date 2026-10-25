import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLI = ROOT / 'tools/kart_assets/toolchain.py'
sys.path.insert(0, str(CLI.parent))
import toolchain


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ToolchainTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = ROOT / 'build/kart-tools'
        base.mkdir(parents=True, exist_ok=True)
        cls.out = Path(tempfile.mkdtemp(prefix='regression-', dir=base))
        cls.run_job('selftest', cls.out / 'fixture')
        cls.source = cls.out / 'fixture/calibration.blend'

    @staticmethod
    def run_job(command, output, blend=None, success=True):
        args = [sys.executable, str(CLI), command, '--out', str(output)]
        if blend:
            args += ['--blend', str(blend)]
        result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
        if (result.returncode == 0) != success:
            raise AssertionError(result.stdout + result.stderr)
        return result

    def test_reference_pins(self):
        for source in json.loads((CLI.parent / 'sources.json').read_text())['sources']:
            self.assertEqual(toolchain.git('rev-parse', 'HEAD', cwd=toolchain.CACHE / source['path']),
                             source['revision'])

    def test_blender_functional_checks(self):
        result = json.loads((self.out / 'fixture/selftest.json').read_text())
        self.assertTrue(result['passed'])
        self.assertEqual(len(result['checks']), 11)

    def test_export_repeatable_and_source_unchanged(self):
        before = digest(self.source)
        for folder in ('export-a', 'export-b'):
            self.run_job('export', self.out / folder, self.source)
        for relative in ('calibration/model.inc.c', 'calibration/header.h', 'manifest.json'):
            self.assertEqual(digest(self.out / 'export-a' / relative), digest(self.out / 'export-b' / relative))
        self.assertEqual(before, digest(self.source))

    def test_empty_template_rejected(self):
        target = self.out / 'empty-validation'
        self.run_job('validate', target, self.out / 'fixture/kart_authoring.blend', success=False)
        self.assertIn('KART_EXPORT needs at least one mesh', json.loads((target / 'inspection.json').read_text())['errors'])

    def test_existing_output_not_overwritten(self):
        target = self.out / 'fixture'
        before = digest(target / 'selftest.json')
        self.run_job('selftest', target, success=False)
        self.assertEqual(before, digest(target / 'selftest.json'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
