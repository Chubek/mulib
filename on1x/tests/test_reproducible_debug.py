#!/usr/bin/env python3
"""Byte-for-byte Linux project dev builds with real GCC/Clang and On1x tools."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ON1X = Path(__file__).resolve().parents[1]
OPTIONS = None


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], capture_output=True, text=True,
                          timeout=180, **kwargs)


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux ELF project build test')
class ReproducibleDebug(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='on1x-repro-debug-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def build_pair(self, mode):
        outputs = []
        checkouts = []
        for name in ('first checkout', 'second checkout'):
            checkout = self.root / name
            checkouts.append(checkout)
            project = checkout / 'app'
            staging = checkout / 'tmp root'
            sdk = checkout / 'sdk'
            (project / 'src').mkdir(parents=True)
            staging.mkdir()
            for path in ('tools', 'runtime', 'stdlib'):
                shutil.copytree(ON1X / path, sdk / path, ignore=shutil.ignore_patterns('__pycache__'))
            (project / 'On1xfile').write_text(
                'on1xfile 1\nkind executable\nname "sample"\n'
                'directory "src"\ntarget "sample" "src/main.on1x"\n')
            (project / 'src/On1xfile.m').write_text(
                'on1xfile 1\nsource "main.on1x"\nlibrary "value.on1x"\n')
            (project / 'src/main.on1x').write_text(
                'import value as V;\nfn main() -> i64 { println(V.number()); return 0; }\n')
            (project / 'src/value.on1x').write_text('fn number() -> i64 { return 42; }\n')
            result = run(sys.executable, sdk / 'tools/on1x-project.py', 'build', project,
                         '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc, '--stdlib', sdk / 'stdlib',
                         *(['--sandbox'] if mode == 'sandbox' else []),
                         env=dict(os.environ, TMPDIR=str(staging)))
            self.assertEqual(result.returncode, 0, result.stderr)
            output = Path(result.stdout.strip())
            program = output / 'sample'
            executed = run(program)
            self.assertEqual((executed.returncode, executed.stdout), (0, '42\n'), executed)
            binary = program.read_bytes()
            report = (output / '.on1x-build.json').read_bytes()
            data = json.loads(report)
            self.assertEqual(data['cflags'], ['-O0', '-g'])
            self.assertEqual(data['outputs']['sample'], hashlib.sha256(binary).hexdigest())
            outputs.append((binary, report))

        for what, (first, second) in zip(('executable', 'build report'), zip(*outputs)):
            left, right = hashlib.sha256(first).hexdigest(), hashlib.sha256(second).hexdigest()
            self.assertTrue(first == second, f'{mode} {what}: {left} != {right}')

        # Do not accept release output or stripping as a reproducibility fix.
        binary = outputs[0][0]
        for marker in (b'.debug_info', b'.debug_line', b'/__on1x__/generated/main.c',
                       b'/__on1x__/sdk/runtime/on1x_runtime.c', b'/__on1x__/build/cwd',
                       b'on1x_fn_main'):
            self.assertIn(marker, binary)
        for path in (*map(os.fsencode, checkouts), b'on1x-project-', b'on1x-build-', b'/work/cwd'):
            self.assertNotIn(path, binary)
        print(f'{mode} executable SHA-256: {hashlib.sha256(binary).hexdigest()}')

    def test_normal(self):
        self.build_pair('normal')

    def test_sandbox(self):
        bwrap = shutil.which('bwrap')
        if not bwrap:
            self.skipTest('bubblewrap is unavailable')
        command = [bwrap, '--unshare-all', '--ro-bind', '/usr', '/usr']
        for path in map(Path, ('/bin', '/sbin', '/lib', '/lib64')):
            if path.is_symlink():
                command.extend(('--symlink', os.readlink(path), path))
            elif path.is_dir():
                command.extend(('--ro-bind', path, path))
        probe = run(*command, '--', '/usr/bin/true')
        if probe.returncode:
            self.skipTest(f'bubblewrap namespaces unavailable: {probe.stderr.strip()}')
        self.build_pair('sandbox')

    def test_single_file(self):
        source = self.root / 'single file.on1x'
        source.write_text('extern fn answer() -> i64;\nfn main() { println(answer()); }\n')
        extra_c = self.root / 'extra source.c'
        extra_c.write_text('#include <stdint.h>\nint64_t answer(void) { return 42; }\n')
        generated = self.root / 'manual.c'
        program = self.root / 'program'
        common = [sys.executable, ON1X / 'tools/on1x-build.py', source.name,
                  '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc]
        result = run(*common, '--emit-only', '-o', generated.name, cwd=self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('on1x_fn_main', generated.read_text())
        result = run(*common, '--extra-c', extra_c.name, '--cflag=-g', '-o', program.name, cwd=self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        executed = run(program)
        self.assertEqual((executed.returncode, executed.stdout), (0, '42\n'), executed)


if __name__ == '__main__':
    cli = argparse.ArgumentParser()
    cli.add_argument('--on1xc', required=True, type=lambda path: Path(path).resolve())
    cli.add_argument('--cc', default='cc')
    OPTIONS, remainder = cli.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remainder], verbosity=2)
