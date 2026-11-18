#!/usr/bin/env python3
"""Exercise the real compiler, Git object store, Mune, and QMR when available."""
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


class ProjectTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='on1x-project-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / 'project'
        self.project.mkdir()
        self.output = self.project / 'target/dev'

    def write(self, path, text):
        path = self.project / path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        return path

    def app(self, extra='', implementation='fn main() -> i64 { println(42); return 0; }\n'):
        self.write('On1xfile', 'on1xfile 1\nkind executable\nname "sample"\n'
                   'directory "src"\ntarget "sample" "src/main.on1x"\n' + extra)
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\n')
        self.write('src/main.on1x', implementation)

    def build(self, *arguments, negative=None, root=None, env=None):
        cmd = [sys.executable, OPTIONS.driver, 'build', str(root or self.project),
               '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc]
        for flag in ('mune_runner', 'qmr_compiler', 'mulib_root'):
            value = getattr(OPTIONS, flag)
            if value:
                cmd.extend(('--' + flag.replace('_', '-'), value))
        cmd.extend(arguments)
        result = subprocess.run(cmd, text=True, capture_output=True, env=env, timeout=180)
        if negative is None:
            self.assertEqual(result.returncode, 0, result.stderr)
            return Path(result.stdout.strip())
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(negative, result.stderr)

    def library(self, prefix, label):
        self.write(f'{prefix}/On1xfile', f'on1xfile 1\nname "{label}"\nkind library\ndirectory "lib"\n')
        self.write(f'{prefix}/lib/On1xfile.m', 'on1xfile 1\nlibrary "value.on1x"\n')
        self.write(f'{prefix}/lib/On1xfile.i', 'on1xfile 1\ninterface "value.on1xi"\n')
        self.write(f'{prefix}/lib/value.on1xi', 'on1xi 1;\nfn number() -> i64;\n')
        self.write(f'{prefix}/lib/value.on1x', 'fn number() -> i64 { return 42; }\n')
        return self.project / prefix

    def run_bin(self, output=None, target='sample'):
        result = subprocess.check_output([str((output or self.output) / target)], text=True, timeout=5)
        self.assertEqual(result, '42\n')

    def test_basic_and_transaction(self):
        self.app()
        self.assertEqual(self.build(), self.output)
        self.run_bin()
        report = self.output / '.on1x-build.json'
        before = report.read_bytes()
        self.assertEqual(json.loads(before)['cflags'], ['-O0', '-g'])
        self.write('src/main.on1x', 'fn main() -> i64 { return does_not_exist(); }')
        self.build(negative='unknown function')
        self.assertEqual(report.read_bytes(), before)
        self.run_bin()
        self.write('src/main.on1x', 'fn main() -> i64 { println(42); return 0; }\n')
        release = self.build('--release')
        self.run_bin(release)
        self.assertEqual(json.loads((release / report.name).read_text())['cflags'], ['-O3'])
        self.build('--release', '--output-dir', str(self.output),
                   negative='not a build output for this project kind/name/profile')

    def test_only_declared_roles_and_imports(self):
        self.app(implementation='import private as P;\nfn main() -> i64 { println(P.number()); return 0; }\n')
        self.write('src/private.on1x', 'fn number() -> i64 { return 42; }\n')
        self.build(negative='module not found: private')
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nlibrary "private.on1x"\n')
        self.build()
        self.run_bin()
        self.write('src/On1xfile.m', 'on1xfile 1\nlibrary "main.on1x"\nlibrary "private.on1x"\n')
        self.build(negative='must name a locally declared source')
        self.write('On1xfile', 'on1xfile 1\nkind library\nname "sample"\nlibrary "src/private.on1x"\n')
        self.build(negative="unsupported 'library' in root")

    def test_bad_paths_ppx_and_ambient_environment(self):
        self.app(implementation='import unexpected;\nfn main() -> i64 { return 0; }\n')
        outside = self.root / 'unexpected.on1x'
        outside.write_text('fn value() -> i64 { return 42; }')
        self.build(negative='module not found: unexpected', env=dict(os.environ, ON1X_LIB_PATH=str(self.root),
                                                                     ON1X_PATH=str(self.root), CPATH='/absent'))
        self.write('src/main.on1x', f'import "{outside}";\nfn main() -> i64 {{ return 0; }}\n')
        self.build(negative='expected a normalized local path')
        self.write('src/main.on1x', '@ppx bad "/bin/false";\nfn main() -> i64 { return 0; }\n')
        self.build(negative='source PPX and executable extensions')
        file = self.project / 'src/main.on1x'
        file.unlink()
        file.symlink_to(outside)
        self.build(negative='symlinks are not allowed')

    def test_complex_targets(self):
        self.app()
        first = (self.project / 'On1xfile').read_text().replace('kind executable', 'kind complex')
        self.write('On1xfile', first + 'target "other" "src/other.on1x"\n')
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nsource "other.on1x"\n')
        self.write('src/other.on1x', 'fn main() -> i64 { println(42); return 0; }\n')
        self.build()
        self.run_bin(target='other')
        self.run_bin()
        partial = self.build('--target', 'other', '--output-dir', str(self.root / 'selected'))
        self.assertFalse((partial / 'sample').exists())
        self.run_bin(partial, 'other')
        self.build('--target', 'nope', negative='unknown target')

    def test_local_and_source_library(self):
        dependency = self.library('vendor/toolkit', 'toolkit')
        self.app(extra='dependency "toolkit" local "vendor/toolkit"\n',
                 implementation='import toolkit.value as V;\nfn main() -> i64 { println(V.number()); return 0; }\n')
        self.build()
        self.run_bin()
        package = self.build(root=dependency) / 'library'
        self.assertTrue((package / 'lib/value.on1xi').is_file())
        shutil.copytree(package, self.project / 'vendor/republished')
        manifest = (self.project / 'On1xfile').read_text().replace('vendor/toolkit', 'vendor/republished')
        self.write('On1xfile', manifest)
        self.build()
        self.run_bin()
        self.write('vendor/republished/lib/On1xfile.m',
                   'on1xfile 1\n{% error("must not execute") %}\nlibrary "value.on1x"\n')
        self.build(negative='dependency manifests must be plain declarative text')

    @unittest.skipUnless(shutil.which('git'), 'Git is unavailable')
    def test_pinned_git_ignores_checkout_and_config(self):
        dependency = self.library('vendor/repository', 'toolkit')
        git = ['git', '-C', str(dependency)]
        env = dict(os.environ, GIT_CONFIG_GLOBAL='/dev/null', GIT_CONFIG_NOSYSTEM='1')
        args = [*git, '-c', 'user.name=Build test', '-c', 'user.email=build@localhost']
        subprocess.run([*args, 'init', '-q'], check=True, env=env)
        subprocess.run([*args, 'add', 'On1xfile', 'lib'], check=True, env=env)
        subprocess.run([*args, 'commit', '-qm', 'library'], check=True, env=env)
        commit = subprocess.check_output([*args, 'rev-parse', 'HEAD'], text=True, env=env).strip()
        subprocess.run([*args, 'gc', '--quiet'], check=True, env=env)
        self.app(extra=f'dependency "toolkit" git "vendor/repository" "{commit}"\n',
                 implementation='import toolkit.value as V;\nfn main() -> i64 { println(V.number()); return 0; }\n')
        self.write('vendor/repository/lib/value.on1x', 'not in the pinned revision\n')
        self.write('vendor/repository/lib/value.on1xi', 'not in the pinned revision\n')
        self.write('vendor/repository/.git/config', '[invalid config file\n')
        self.write('vendor/repository/.git/objects/info/alternates', str(self.root / 'missing') + '\n')
        self.build()
        self.run_bin()
        report = json.loads((self.output / '.on1x-build.json').read_text())
        self.assertEqual(report['dependencies']['toolkit']['commit'], commit)
        original = (self.project / 'On1xfile').read_text()
        self.write('On1xfile', original.replace(commit, 'HEAD'))
        self.build(negative='requires a full lowercase commit')
        self.write('On1xfile', original.replace(commit, '0' * len(commit)))
        self.build(negative='is missing from the offline object store')

    def test_profiles_and_preprocessing(self):
        if not (OPTIONS.mune_runner and OPTIONS.qmr_compiler):
            self.skipTest('requires optional MuLIB Mune and real Moonstone QMR helpers')
        self.app(extra='profile "small" "profile.lua"\ndefine entry "main.on1x"\n')
        self.write('profile.lua', 'assert(io == nil and os == nil and package == nil and math.random == nil)\n'
                   'return on1xfile.profile { optimize = 1, debug = true }\n')
        self.write('src/On1xfile.m', 'on1xfile 1\n{% if data.profile == "small" then %}'
                   'source {{ data.entry }}\n{% end %}')
        self.write('src/On1xfile.i', 'on1xfile 1\n{# compiled by real QMR, evaluated by Mune #}\n'
                   'interface {{ "main.on1xi" }}\n')
        self.write('src/main.on1xi', 'on1xi 1;\nfn main() -> i64;\n')
        output = self.build('--profile', 'small')
        self.run_bin(output)
        report = json.loads((output / '.on1x-build.json').read_text())
        self.assertEqual(report['cflags'], ['-O1', '-g'])
        self.assertIn('project/src/On1xfile.i (rendered)', report['inputs'])
        self.write('profile.lua', 'return { optimize = 1, debug = true, unexpected = 3 }\n')
        self.build('--profile', 'small', negative='invalid or exhausted build policy')
        self.write('profile.lua', 'while true do end\n')
        self.build('--profile', 'small', negative='invalid or exhausted build policy')

    def test_explicit_shell(self):
        if not (OPTIONS.mune_runner and OPTIONS.qmr_compiler):
            self.skipTest('requires optional MuLIB Mune and real Moonstone QMR helpers')
        self.app(extra='define entry shell "printf main.on1x"\n')
        self.write('src/On1xfile.m', 'on1xfile 1\nsource {{ data.entry }}\n')
        self.build(negative='requires explicit --allow-shell')
        self.build('--sandbox', '--allow-shell', negative='cannot be combined')
        self.build('--allow-shell')
        self.run_bin()

    def test_sandbox_and_repeatability(self):
        bwrap = shutil.which('bwrap')
        if not bwrap:
            self.skipTest('bubblewrap is unavailable')
        probe = subprocess.run([bwrap, '--unshare-all', '--ro-bind', '/usr', '/usr',
                                '--ro-bind', '/lib64', '/lib64', '/usr/bin/true'],
                               capture_output=True, timeout=5)
        if probe.returncode:
            self.skipTest('user or network namespaces are unavailable')
        self.app(implementation='fn main() -> i64 { println(42); return 0; }\n')
        marker = self.root / 'not-mounted'
        marker.touch()
        outside_netns = os.readlink('/proc/self/ns/net')
        custom_cc = self.root / 'sandbox-cc'
        path = shutil.which(OPTIONS.cc) or OPTIONS.cc
        custom_cc.write_text('#!/bin/sh\n'
                             f"test ! -e '{marker}' || exit 10\n"
                             f"test \"$(readlink /proc/self/ns/net)\" != '{outside_netns}' || exit 11\n"
                             f"exec '{path}' \"$@\"\n")
        custom_cc.chmod(0o755)
        one = self.build('--sandbox', '--release', '--cc', str(custom_cc))
        self.run_bin(one)
        another = self.root / 'another'
        # Copy only source/project data, not the first build's output.
        shutil.copytree(self.project, another, ignore=shutil.ignore_patterns('target'))
        two = self.build('--sandbox', '--release', '--cc', str(custom_cc), root=another)
        self.run_bin(two)
        self.assertEqual(hashlib.sha256((one / 'sample').read_bytes()).digest(),
                         hashlib.sha256((two / 'sample').read_bytes()).digest())

    def test_mulib_native_plan(self):
        self.app(extra='kernel "clamp"\n')
        self.build()
        self.run_bin()
        plan = json.loads((self.output / 'sample.mulib.json').read_text())
        self.assertEqual(plan['plan']['implementation_macros'], ['MULIB_IMPL_CLAMP'])

    def test_manifest_format_without_execution(self):
        self.app()
        path = self.write('src/On1xfile.m', '  on1xfile 1\n\t# note\n  source   "main.on1x" # entry\n\n')
        cli = [sys.executable, OPTIONS.driver, 'format', str(path)]
        clean = 'on1xfile 1\n# note\nsource "main.on1x" # entry\n'
        self.assertNotEqual(subprocess.run([*cli, '--check'], capture_output=True).returncode, 0)
        self.assertEqual(subprocess.check_output(cli, text=True), clean)
        path.write_text(clean)
        self.assertEqual(subprocess.run([*cli, '--check'], capture_output=True).returncode, 0)
        path.write_text('on1xfile 1\n{% error("not executed") %}\n')
        self.assertIn('no template code was executed',
                      subprocess.run(cli, capture_output=True, text=True).stderr)


if __name__ == '__main__':
    cli = argparse.ArgumentParser()
    cli.add_argument('--on1xc', required=True)
    cli.add_argument('--cc', default='cc')
    cli.add_argument('--driver', default=str(ON1X / 'tools/on1x-project.py'))
    cli.add_argument('--mune-runner')
    cli.add_argument('--qmr-compiler')
    cli.add_argument('--mulib-root')
    OPTIONS, remainder = cli.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remainder], verbosity=2)
