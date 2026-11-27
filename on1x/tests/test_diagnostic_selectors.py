#!/usr/bin/env python3
"""Presentation policy uses exact LibStone selectors without changing admission."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ON1X = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--on1xc', type=Path, required=True)
parser.add_argument('--cc', required=True)
parser.add_argument('--json', action='store_true')
options = parser.parse_args()
OFF = ['--diagnostic-disable', 'all']
IMPORT = 'ON1X-UNUSED-IMPORT'
VARIABLE = 'ON1X-UNUSED-VARIABLE'
TYPE = 'ON1X-UNUSED-TYPE'
ALIAS = 'ON1X-IMPORT-ALIAS'
MODULE = ('module Lib;\nprivate record Hidden { n: i64 }\n'
          'public fn answer() -> i64 { return 42; }\n')
SOURCE = 'import Lib as spare;\nfn main() { let unneeded = 1; let _ = 2; }\n'


class DiagnosticSelectors(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.write(self.root, 'src/main.on1x', SOURCE)
        self.lib = self.write(self.root, 'src/Lib.on1xm', MODULE)

    @staticmethod
    def write(root, filename, content):
        path = root / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')
        return path

    def run_command(self, *argv, status=0):
        result = subprocess.run(argv, cwd=self.root, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, status, result)
        return result

    def compile(self, *args, status=0):
        return self.run_command(options.on1xc, '--stdlib', ON1X / 'stdlib', *args, self.source, status=status)

    def manifest(self, root, policy='', extra=''):
        self.write(root, 'On1xfile', 'on1xfile 1\nkind executable\nname "demo"\ndirectory "src"\n'
                   'target "demo" "src/main.on1x"\nmodule "Lib" "src/Lib.on1xm"\n' + policy + extra)
        self.write(root, 'src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\n')

    def project(self, root, output, cache, *flags, status=0, action='build'):
        return self.run_command(sys.executable, ON1X / 'tools/on1x-project.py', action, root,
                                '--on1xc', options.on1xc, '--cc', options.cc, '--stdlib', ON1X / 'stdlib',
                                '--output-dir', output, '--module-cache', cache, *flags, status=status)

    def test_exact_specificity_and_order(self):
        def shown(*flags):
            return self.compile('--check', *flags).stderr

        default = shown()
        for code in (IMPORT, VARIABLE, TYPE):
            self.assertIn(code, default)
        self.assertEqual(shown(*OFF), '')
        self.assertEqual(shown('--diagnostic-disable', 'warning', '--diagnostic-disable', 'error',
                               '--diagnostic-disable', 'info'), '')
        only_import = shown(*OFF, '--diagnostic-enable', 'name:unused-import',
                            '--diagnostic-disable', 'group:unused')
        self.assertIn(IMPORT, only_import)
        self.assertNotIn(VARIABLE, only_import)
        self.assertNotIn(TYPE, only_import)
        group = shown(*OFF, '--diagnostic-disable', 'name:unused-import',
                      '--diagnostic-enable', 'group:unused')
        self.assertNotIn(IMPORT, group)
        self.assertIn(VARIABLE, group)
        self.assertIn(TYPE, group)
        self.assertEqual(shown(*OFF, '--diagnostic-enable', 'name:Unused-import'), '')
        self.assertEqual(shown(*OFF, '--diagnostic-enable', 'name:unused'), '')
        self.assertEqual(shown('--diagnostic-enable', 'name:unused-import', *OFF), '')
        self.assertEqual(shown(*OFF, '--diagnostic-enable', 'group:unused',
                               '--diagnostic-disable', 'group:unused'), '')
        invalid = self.compile('--check', '--diagnostic-enable', 'name:unused-*', status=2)
        self.assertIn('expected diagnostic selector', invalid.stderr)

    def test_hidden_error_has_no_artifact_and_can_be_selected(self):
        self.source.write_text(SOURCE.replace('as spare', 'as _'), encoding='utf-8')
        output = self.root / 'invalid.c'
        result = self.compile('--emit-c', '-o', output, *OFF, status=1)
        self.assertEqual((result.stdout, result.stderr), ('', ''))
        self.assertFalse(output.exists())
        for selector in ('name:invalid-import-alias', 'group:imports'):
            form = ('--diagnostics', 'json') if options.json else ()
            error = self.compile('--emit-c', '-o', output, *OFF, '--diagnostic-enable', selector,
                                 *form, status=1)
            self.assertFalse(output.exists())
            if options.json:
                (record,) = json.loads(error.stderr)['diagnostics']
                self.assertEqual((record['id'], record['name'], record['group']),
                                 (ALIAS, 'invalid-import-alias', 'imports'))
            else:
                self.assertIn(ALIAS, error.stderr)

    def test_structured_and_info(self):
        if not options.json:
            self.skipTest('structured diagnostics unavailable')
        flags = (*OFF, '--diagnostic-enable', 'name:unused-variable')
        result = self.compile('--check', '--diagnostics', 'json', *flags)
        (record,) = json.loads(result.stderr)['diagnostics']
        self.assertEqual((record['id'], record['severity'], record['name'], record['group']),
                         (VARIABLE, 'warning', 'unused-variable', 'unused'))
        sarif = self.compile('--check', '--diagnostics', 'sarif', *flags)
        self.assertEqual([item['ruleId'] for run in json.loads(sarif.stderr)['runs'] for item in run['results']],
                         [VARIABLE])
        change = self.root / 'already-valid.on1x'
        result = self.compile('--repair', '-o', change, '--diagnostic-disable', 'info')
        self.assertEqual(result.stderr, '')
        self.assertFalse(change.exists())
        result = self.compile('--repair', '-o', change, *OFF, '--diagnostic-enable', 'name:already-valid')
        self.assertTrue(result.stderr)
        self.assertFalse(change.exists())

    def test_project_order_and_cache_isolation(self):
        root, cache, output = self.root, self.root / 'cache', self.root / 'result'
        # The trusted module cache cannot live inside a project.
        project = root / 'project'
        self.write(project, 'src/main.on1x', SOURCE)
        self.write(project, 'src/Lib.on1xm', MODULE)
        self.manifest(project, 'diagnostic all off\n')
        self.assertEqual(self.project(project, output, cache).stderr, '')
        markers = {path.name: path.read_bytes() for path in cache.iterdir()}
        self.assertTrue(markers)
        self.assertEqual(self.project(project, output, cache).stderr, '')
        self.assertEqual(markers, {path.name: path.read_bytes() for path in cache.iterdir()})

        policy = 'diagnostic all off\ndiagnostic "group:unused" on\ndiagnostic "name:unused-import" off\n'
        self.manifest(project, policy)
        flags = ('--diagnostic-enable', 'name:unused-import', '--diagnostic-disable', 'group:unused')
        selected = self.project(project, output, cache, *flags).stderr
        self.assertIn(IMPORT, selected)
        self.assertNotIn(VARIABLE, selected)
        self.assertNotIn(TYPE, selected)
        self.assertEqual(selected, self.project(project, output, cache, *flags).stderr)
        self.assertNotIn('on1x-project-', selected)
        plan = json.loads(self.project(project, output, cache, *flags, action='plan').stdout)
        self.assertEqual(plan['diagnostic_selectors'], [
            ['all', False], ['group:unused', True], ['name:unused-import', False],
            ['name:unused-import', True], ['group:unused', False]])
        # A second project sharing the cache does not inherit the first policy.
        second = root / 'second'
        self.write(second, 'src/main.on1x', SOURCE)
        self.write(second, 'src/Lib.on1xm', MODULE)
        self.manifest(second, 'diagnostic all off\ndiagnostic "name:unused-type" on\n')
        for _ in range(2):
            shown = self.project(second, root / 'second-result', cache).stderr
            self.assertIn(TYPE, shown)
            self.assertNotIn(IMPORT, shown)
            self.assertNotIn(VARIABLE, shown)

        before = {path.name: path.read_bytes() for path in cache.iterdir()}
        self.write(project, 'src/Lib.on1xm', MODULE.replace('return 42', 'return true'))
        invalid = root / 'no-generation'
        hidden = self.project(project, invalid, cache, *OFF, status=1)
        self.assertEqual((hidden.stdout, hidden.stderr), ('', ''))
        self.assertFalse(invalid.exists())
        self.assertEqual(before, {path.name: path.read_bytes() for path in cache.iterdir()})

    def test_local_and_dependency_manifest_have_no_authority(self):
        project, cache, output = self.root / 'project', self.root / 'cache', self.root / 'out'
        self.write(project, 'src/main.on1x', SOURCE)
        self.write(project, 'src/Lib.on1xm', MODULE)
        self.manifest(project)
        self.write(project, 'src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\ndiagnostic all off\n')
        local = self.project(project, output, cache, status=1)
        self.assertIn('only allowed in the root project On1xfile', local.stderr)
        self.manifest(project, extra='dependency "dep" local "deps/dep"\n')
        self.write(project, 'deps/dep/On1xfile', 'on1xfile 1\nkind library\nname "dep"\n'
                   'diagnostic all off\n')
        dependency = self.project(project, output, cache, status=1)
        self.assertIn('only allowed in the root project On1xfile', dependency.stderr)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]])
