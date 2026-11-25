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

    def kernel_paths(self):
        first, second = self.project / 'vendor/first', self.project / 'second root'
        first.mkdir(parents=True)
        second.mkdir()
        (first / 'mulib-example.h').write_text('#include "value.inc"\n')
        (first / 'value.inc').write_text('int example(void);\n')
        (second / 'mulib-example.h').write_text('int other(void);\n')
        manifest = self.project / 'On1xfile'
        manifest.write_text(manifest.read_text() + 'kernel-path\n  "vendor/first"\n'
                            'kernel-path "second root" # lower precedence\n')
        return manifest, first, second

    def project_command(self, command, *options, success=True, env=None):
        result = subprocess.run([sys.executable, '-I', DRIVER, command, self.project, *options],
                                env=env, text=True, capture_output=True, timeout=90)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_kernel_paths_are_root_only_and_contained(self):
        manifest, first, _ = self.kernel_paths()
        original = manifest.read_bytes()
        config = ENGINE.root_manifest(original, manifest)
        self.assertEqual(config['kernel_paths'], [Path('vendor/first'), Path('second root')])
        for path in ('/usr/include', '../outside', 'vendor/../outside', 'vendor//first', '.', 'vendor/first'):
            with self.subTest(path=path):
                with self.assertRaisesRegex(ValueError, 'normalized local path|duplicate declaration'):
                    ENGINE.root_manifest(original + f'kernel-path "{path}"\n'.encode(), manifest)
        with self.assertRaisesRegex(ValueError, 'root project On1xfile'):
            ENGINE.root_manifest(b'on1xfile 1\nkind library\nname "lib"\nkernel-path "headers"\n',
                                 'dependency/lib/On1xfile', dependency=True)
        with self.assertRaisesRegex(ValueError, 'root project On1xfile'):
            ENGINE.local_roles(b'on1xfile 1\nkernel-path "headers"\n', 'On1xfile.i', Path('On1xfile.i'), None)
        with self.assertRaisesRegex(ValueError, 'nested/On1xfile:3:.*root project'):
            ENGINE.check_nested_authority(b'{% if true then %}\n# comment\nkernel-path "headers"\n',
                                          'nested/On1xfile')
        manifest.write_bytes(original + b'dependency "dep" local "vendor"\n')
        self.assertIn('cannot overlap', self.project_command('lock', success=False).stderr)
        manifest.write_bytes(original)

        outside = self.root / 'outside'
        outside.mkdir()
        (outside / 'leaked.h').touch()
        symlink = first / 'linked.h'
        symlink.symlink_to(outside / 'leaked.h')
        self.assertIn('symlinks are not allowed', self.project_command('lock', success=False).stderr)
        symlink.unlink()
        symlink = first / 'dir'
        symlink.symlink_to(outside, target_is_directory=True)
        self.assertIn('symlinks are not allowed', self.project_command('lock', success=False).stderr)
        symlink.unlink()
        os.mkfifo(first / 'linked.h')
        self.assertIn('expected a regular file or directory', self.project_command('lock', success=False).stderr)
        (first / 'linked.h').unlink()
        manifest.write_bytes(original + b'kernel-path "link/leaf"\n')
        (self.project / 'link').symlink_to(outside, target_is_directory=True)
        self.assertIn('real directory tree without symlinks',
                      self.project_command('lock', success=False).stderr)

    def test_kernel_paths_lock_order_contents_and_stay_out_of_compiler_environment(self):
        manifest, first, _ = self.kernel_paths()
        original = manifest.read_bytes()
        formatted = subprocess.check_output([sys.executable, '-I', DRIVER, 'format', manifest])
        self.assertEqual(formatted, original)
        tool_args = ('--on1xc', ARGS.on1xc, '--cc', ARGS.cc)
        self.assertIn('lockfile is required',
                      self.project_command('plan', '--frozen', *tool_args, success=False).stderr)
        lock = Path(self.project_command('lock').stdout.strip())
        locked = json.loads(lock.read_bytes())
        self.assertEqual(locked['schema'], 'on1x.lock/v4')
        self.assertEqual([row['path'] for row in locked['kernel_paths']], ['vendor/first', 'second root'])
        compiler = self.script(self.root / 'compiler-no-roots',
                               'test "${MULIB_KERNELS_PATH-unset}:${MULIB_FOREIGNLIB_PATH-unset}" = unset:unset\n'
                               f'exec {shlex.quote(ARGS.on1xc)} "$@"\n')
        env = dict(os.environ, MULIB_KERNELS_PATH='/ambient-headers', MULIB_FOREIGNLIB_PATH='/ambient-recipes')
        selected = ('--on1xc', compiler, '--cc', ARGS.cc, '--frozen')
        plan = json.loads(self.project_command('plan', *selected, env=env).stdout)
        self.assertEqual(plan['kernel_paths'], locked['kernel_paths'])
        self.assertEqual(plan['inputs']['kernel-path/0/value.inc'],
                         hashlib.sha256((first / 'value.inc').read_bytes()).hexdigest())
        dest = Path(self.project_command('build', *selected, env=env).stdout.strip())
        report = json.loads((dest / '.on1x-build.json').read_bytes())
        self.assertEqual(report['kernel_paths'], plan['kernel_paths'])
        self.assertEqual(report['inputs'], plan['inputs'])
        with self.subTest('root order'):
            manifest.write_bytes(original.split(b'kernel-path', 1)[0] +
                                 b'kernel-path "second root"\nkernel-path "vendor/first"\n')
            self.assertIn('stale kernel-path', self.project_command('plan', *selected, success=False).stderr)
            manifest.write_bytes(original)
        with self.subTest('a non-header include changes'):
            (first / 'value.inc').write_text('int changed(void);\n')
            self.assertIn('stale kernel-path', self.project_command('lock', '--frozen', success=False).stderr)
            self.assertIn('stale kernel-path',
                          self.project_command('plan', *tool_args, success=False).stderr)
            self.project_command('lock')
            new = json.loads(self.project_command('plan', *selected).stdout)
            self.assertNotEqual(new['kernel_paths'][0]['sha256'], plan['kernel_paths'][0]['sha256'])
            self.assertEqual(new['kernel_paths'][1]['sha256'], plan['kernel_paths'][1]['sha256'])
        self.assertIn('cannot overlap',
                      self.project_command('build', *selected, '--output-dir', first, success=False).stderr)

    def ffi_generation_handoff(self, sandbox):
        _, first, second = self.kernel_paths()
        self.project_command('lock')
        # Stand-in for the canonical subprocess solely to assert the driver
        # protocol without requiring a Cephyr SDK in this hermeticity suite.
        toolkit = self.root / 'toolkit'
        handler, cephyr = toolkit / 'handler', toolkit / 'cephyr'
        handler.mkdir(parents=True)
        cephyr.mkdir()
        script = handler / 'ffispec-gen.py'
        script.write_text(
            'import os, sys\n'
            'from pathlib import Path\n'
            'output, library, config = map(Path, sys.argv[2::2])\n'
            'assert sys.argv[1::2] == ["--output-dir", "--cephyr-library", "--config"]\n'
            'assert library.read_text() == "test library" and config.read_text() == "test config"\n'
            'assert os.environ["HOME"] == "/nonexistent"\n'
            'assert sorted(os.environ.keys() & {"CC", "CPATH", "CEPHYR_LIBRARY", "MULIB_FOREIGNLIB_PATH"}) == []\n'
            'paths = [Path(p).resolve() for p in os.environ["MULIB_KERNELS_PATH"].split(os.pathsep)]\n'
            'assert [p.name for p in paths] == ["0", "1"]\n'
            'assert all(p.parent == Path.cwd().parent / "input/kernel-inputs" for p in paths)\n'
            'assert [p.joinpath("mulib-example.h").read_text() for p in paths] == '
            '[\'#include "value.inc"\\n\', "int other(void);\\n"]\n'
            'assert (paths[0]/"value.inc").is_file()\n'
            + ('assert Path.cwd() == Path("/work/cwd")\n'
               'assert not os.access(paths[0]/"value.inc", os.W_OK)\n'
               f'assert not Path({str(self.project)!r}).exists()\n'
               f'assert os.readlink("/proc/self/ns/net") != {os.readlink("/proc/self/ns/net")!r}\n'
               if sandbox else '') +
            'output.joinpath("index.json").write_text(\'{"recipes": []}\\n\')\n')
        config = cephyr / 'Cephyr.toml'
        config.write_text('test config')
        library = cephyr / 'libcephyr.so'
        library.write_text('test library')
        dest = self.root / 'generated path'
        options = [str(DRIVER), 'ffi-generate', str(self.project), '--frozen',
                   *(['--sandbox'] if sandbox else []), '--cephyr-library',
                   str(library), '--output-dir', str(dest)]
        with patch.multiple(ENGINE, ROOT=toolkit, FFI_HANDLER=handler, FFI_CEPHYR=cephyr), \
                patch.object(sys, 'argv', options), patch.dict(
                    os.environ, {key: 'ambient' for key in ('MULIB_KERNELS_PATH', 'MULIB_FOREIGNLIB_PATH',
                                                             'CC', 'CPATH', 'CEPHYR_LIBRARY')}):
            output, errors = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                self.assertEqual(ENGINE.main(), 0, errors.getvalue())
            self.assertEqual(Path(output.getvalue().strip()), dest)
            report = json.loads((dest / '.on1x-ffi.json').read_bytes())
            self.assertEqual(report['generation_roots'], ['kernel-inputs/0', 'kernel-inputs/1'])
            self.assertEqual(report['sandbox'], sandbox)
            for source, root in zip((first, second), report['generation_roots']):
                self.assertEqual((dest / root / 'mulib-example.h').read_bytes(),
                                 (source / 'mulib-example.h').read_bytes())
            self.assertEqual(report['outputs']['index.json'],
                             hashlib.sha256((dest / 'index.json').read_bytes()).hexdigest())
            with contextlib.redirect_stderr(errors):
                self.assertEqual(ENGINE.main(), 1)
            self.assertIn('must not already exist', errors.getvalue())
            self.assertEqual(report, json.loads((dest / '.on1x-ffi.json').read_bytes()))
            options[-1] = str(self.root / 'never-published')
            (first / 'value.inc').write_text('int stale;')
            with contextlib.redirect_stderr(errors):
                self.assertEqual(ENGINE.main(), 1)
            self.assertIn('stale kernel-path', errors.getvalue())
            self.assertFalse(Path(options[-1]).exists())

    def test_ffi_generation_handoff_uses_staged_order_and_clean_environment(self):
        self.ffi_generation_handoff(sandbox=False)

    def test_sandboxed_ffi_generation_cannot_see_project_or_mutate_stage(self):
        self.bubblewrap()
        self.ffi_generation_handoff(sandbox=True)

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

    def test_sandbox_mounts_executable_recipe_dirs_without_build_catalogue(self):
        self.bubblewrap()
        prefix = self.root / 'toolchain'
        binary = prefix / 'bin'
        bundle = binary / 'stdffi/mulib'
        built = bundle / 'recipes/clamp.ffi'
        installed = prefix / 'share/on1x/stdffi/mulib/clamp.ffi'
        catalogue = bundle / 'catalogue/clamp.ffi'
        index = bundle / 'index.json'
        for file in (built, installed, catalogue, index):
            file.parent.mkdir(parents=True, exist_ok=True)
            file.touch()
        compiler = self.script(binary / 'on1xc',
                               f'test -f {shlex.quote(str(built))}\n'
                               f'test ! -w {shlex.quote(str(built))}\n'
                               f'test -f {shlex.quote(str(installed))}\n'
                               f'test ! -e {shlex.quote(str(catalogue))}\n'
                               f'test ! -e {shlex.quote(str(index))}\n'
                               'test "${MULIB_FOREIGNLIB_PATH-unset}" = unset\n'
                               'echo "checked sandbox recipe mounts" >&2\nexit 23\n')
        result = self.cli('--sandbox', '--frozen', on1xc=compiler,
                          env=dict(os.environ, MULIB_FOREIGNLIB_PATH=str(catalogue.parent)))
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('checked sandbox recipe mounts', result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--on1xc', required=True)
    parser.add_argument('--cc', default='cc')
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
