#!/usr/bin/env python3
"""Exercise On1xfile's process boundary, separately from lockfile policy."""
import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ON1X = Path(__file__).resolve().parents[1]
DRIVER = ON1X / 'tools/on1x-project.py'
SPEC = importlib.util.spec_from_file_location('_project_hermetic_test', DRIVER)
ENGINE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ENGINE)
ARGS = None


class ProcessBoundary(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='on1x-hermetic-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.project = self.root / 'project'
        self.project.mkdir()
        (self.project / 'On1xfile').write_text(
            'on1xfile 1\nkind executable\nname "sample"\ntarget "sample" "main.on1x"\n')
        (self.project / 'On1xfile.m').write_text('on1xfile 1\nsource "main.on1x"\n')
        (self.project / 'main.on1x').write_text('fn main() -> i64 { return 0; }\n')

    def script(self, path, body):
        path.write_text('#!/bin/sh\nset -eu\n' + body)
        path.chmod(0o755)
        return path

    def cli(self, *options, python=sys.executable, env=None, on1xc=None, cc=None):
        return subprocess.run([python, '-I', DRIVER, 'build', self.project,
                               '--on1xc', on1xc or ARGS.on1xc, '--cc', cc or ARGS.cc, *options],
                              cwd=self.root, env=env, text=True, capture_output=True, timeout=60)

    def bubblewrap(self):
        launcher = shutil.which('bwrap', path=ENGINE.ENV['PATH'])
        if not launcher:
            self.skipTest('bubblewrap unavailable on fixed PATH')
        command = [launcher, '--unshare-all', '--ro-bind', '/usr', '/usr']
        for path in ('/bin', '/lib', '/lib64'):
            if Path(path).is_symlink():
                command.extend(('--symlink', os.readlink(path), path))
            elif Path(path).is_dir():
                command.extend(('--ro-bind', path, path))
        result = subprocess.run([*command, '--', '/usr/bin/true'],
                                text=True, capture_output=True, timeout=10)
        if result.returncode:
            self.skipTest(f'bubblewrap namespaces unavailable: {result.stderr.strip()}')
        return Path(launcher).resolve()

    def test_bare_tool_name_never_falls_back_to_caller_directory(self):
        compiler = self.script(self.root / 'on1x-test-local-compiler',
                               'echo "local compiler was invoked" >&2\nexit 23\n')
        env = dict(os.environ, PATH=str(self.root) + os.pathsep + os.environ.get('PATH', ''))
        result = self.cli('--frozen', on1xc=compiler.name, env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('unavailable on the fixed PATH', result.stderr)
        self.assertNotIn('local compiler was invoked', result.stderr)
        result = self.cli('--frozen', on1xc='./' + compiler.name, env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('local compiler was invoked', result.stderr)

    def test_failed_namespace_setup_does_not_start_compiler_or_publish(self):
        marker = self.root / 'compiler-ran'
        compiler = self.script(self.root / 'compiler', f'touch {shlex.quote(str(marker))}\nexit 23\n')
        launcher = self.script(self.root / 'unavailable-namespace', 'exit 42\n')
        argv = [str(DRIVER), 'build', str(self.project), '--frozen',
                '--sandbox', '--on1xc', str(compiler), '--cc', ARGS.cc]
        stderr = io.StringIO()
        # A failing launcher, not a substitute sandbox. This check also runs
        # on hosts where the real namespace setup is unavailable.
        program = ENGINE.program
        with patch.object(sys, 'argv', argv), contextlib.redirect_stderr(stderr), \
                patch.object(ENGINE, 'program',
                             side_effect=lambda value: launcher if value == 'bwrap' else program(value)):
            self.assertEqual(ENGINE.main(), 1)
        self.assertIn('unavailable-namespace failed (42)', stderr.getvalue())
        self.assertFalse(marker.exists())
        self.assertFalse((self.project / 'target').exists())

    def test_stdlib_discovery_uses_sandbox_and_exact_argv(self):
        self.bubblewrap()
        standard = self.root / 'explicit stdlib'
        standard.mkdir()
        marker = self.project / 'undeclared'
        marker.touch()
        host_net = shlex.quote(os.readlink('/proc/self/ns/net'))
        compiler = self.script(self.root / 'compiler ; name',
                               f'test ! -e {shlex.quote(str(marker))}\n'
                               'test "$PWD" = /work/cwd\n'
                               'test "$HOME" = /nonexistent\n'
                               f'test "$(readlink /proc/self/ns/net)" != {host_net}\n'
                               'test "${ON1X_STDLIB-unset}:${CC-unset}:${CPATH-unset}" = unset:unset:unset\n'
                               'if [ "$1" = --print-stdlib ]; then\n'
                               '    test "$#" = 3 && test "$2" = --stdlib\n'
                               f'    test "$3" = {shlex.quote(str(standard))} && test -d "$3"\n'
                               '    printf "%s\\n" "$3"\n'
                               'else\n'
                               '    echo "sandbox reached optimization inventory" >&2\n'
                               '    exit 23\n'
                               'fi\n')
        result = self.cli('--sandbox', '--frozen', '--stdlib', standard, on1xc=compiler,
                          env=dict(os.environ, ON1X_STDLIB='ambient', CC='ambient', CPATH='ambient'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('sandbox reached optimization inventory', result.stderr)
        self.assertFalse((self.project / 'target').exists())

    def test_sandboxed_frozen_c11_through_python_and_cc_aliases(self):
        bwrap = self.bubblewrap()
        python = self.root / 'python alias'
        python.symlink_to(sys.executable)
        compiler = Path(shutil.which(ARGS.cc) or ARGS.cc).resolve()
        cc = self.script(self.root / 'cc ; wrapper',
                         'test "$PWD" = /work/cwd\n'
                         'test "$PATH" = /usr/bin:/bin\n'
                         'test "${CPATH-unset}:${LIBRARY_PATH-unset}" = unset:unset\n'
                         'test -f /work/input/project/main.on1x && test ! -w /work/input/project/main.on1x\n'
                         f'exec {shlex.quote(str(compiler))} "$@"\n')
        # Directly inspect the sandbox envelope with a host-only staging
        # sibling, then exercise the public CLI through a real Python alias.
        work = self.root / 'staging'
        work.mkdir()
        (work / 'private-host-data').touch()
        for folder in ('cwd', 'input', 'artifact'):
            (work / folder).mkdir()
        envelope = ENGINE.Execution(argparse.Namespace(sandbox=True),
                                    work, Path(ARGS.on1xc).resolve(), cc, ON1X.parent)
        envelope.run(['/bin/sh', '-ec', 'test ! -e /work/private-host-data'])

        result = self.cli('--sandbox', '--frozen', cc=cc, python=python,
                          env=dict(os.environ, PATH=str(self.root), CPATH='ambient', LIBRARY_PATH='ambient'))
        self.assertEqual(result.returncode, 0, result.stderr)
        output = Path(result.stdout.strip())
        subprocess.run([output / 'sample'], check=True, timeout=5)
        report = json.loads((output / '.on1x-build.json').read_bytes())
        self.assertTrue(report['sandbox'])
        for name, path in (('cc', cc), ('bubblewrap', bwrap)):
            self.assertEqual(report['tools'][name], hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--on1xc', required=True)
    parser.add_argument('--cc', default='cc')
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
