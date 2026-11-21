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

    def lock(self, *arguments, negative=None):
        command = [sys.executable, OPTIONS.driver, 'lock', str(self.project), *arguments]
        result = subprocess.run(command, text=True, capture_output=True, timeout=100)
        if negative is None:
            self.assertEqual(result.returncode, 0, result.stderr)
            return Path(result.stdout.strip())
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(negative, result.stderr)

    def run_bin(self, output=None, target='sample', expected='42\n'):
        result = subprocess.check_output([str((output or self.output) / target)], text=True, timeout=5)
        self.assertEqual(result, expected)

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

    def test_scrutiny_precedence(self):
        probe = self.root / 'analysis-probe.on1x'
        probe.write_text('fn main()->i64{return 42/0;}\n')
        checked = subprocess.run(
            [OPTIONS.on1xc, '--no-prelude', '--check', '--scrutiny', 'strict', probe],
            text=True, capture_output=True, timeout=20)
        if checked.returncode == 0:
            self.skipTest('Wetstone analysis is unavailable in this build')
        self.assertIn('ON1X-ANALYSIS-DIVZERO', checked.stderr)

        division = 'fn main()->i64{return 42/0;}\n'
        self.app(extra='scrutiny strict\n', implementation=division)
        self.build(negative='ON1X-ANALYSIS-DIVZERO')

        self.app(extra='scrutiny strict\n', implementation='@scrutiny off;\n' + division)
        output = self.build()
        self.assertEqual(json.loads((output / '.on1x-build.json').read_text())['scrutiny'], 'strict')

        self.app(extra='scrutiny off\n', implementation='@scrutiny strict;\n' + division)
        self.build(negative='ON1X-ANALYSIS-DIVZERO')
        output = self.build('--scrutiny', 'off')
        self.assertEqual(json.loads((output / '.on1x-build.json').read_text())['scrutiny'], 'off')

        self.app(extra='scrutiny off\n', implementation='@scrutiny off;\n' + division)
        self.build('--scrutiny', 'strict', negative='ON1X-ANALYSIS-DIVZERO')

    def test_optimizer_policy_plan_lock_and_build(self):
        program = ('fn plus(x:i64)->i64{return (x+0)*1;}'
                   'fn main(){println(plus(40+2));}\n')
        rules = ('optimization-profile debug\n'
                 'optimization "integer_add_zero_right" on\n')
        self.app(extra=rules, implementation=program)
        lock = self.lock('--on1xc', OPTIONS.on1xc)
        self.assertEqual(json.loads(lock.read_bytes())['optimization'],
                         {'optimization-profile': 'debug', 'selections': {'integer_add_zero_right': True}})
        plan = json.loads(subprocess.check_output(
            [sys.executable, OPTIONS.driver, 'plan', self.project, '--frozen', '--on1xc', OPTIONS.on1xc],
            text=True, timeout=60))
        self.assertFalse(self.output.exists())
        self.assertEqual(plan['targets'], {'sample': 'src/main.on1x'})
        def built(*args):
            out = self.build('--frozen', *args)
            self.run_bin(out)
            return json.loads((out / '.on1x-build.json').read_text())
        first = built()
        self.assertEqual(first['optimization'], plan['optimization'])
        def enabled(record):
            return {item['name'] for item in record['optimization']['optimizations'] if item['enabled']}
        self.assertEqual(enabled(first), {'integer_add_zero_right'})
        second = built('--disable-opt', 'all')
        self.assertEqual(enabled(second), set())
        self.assertNotEqual(first['outputs']['sample'], second['outputs']['sample'])
        mir = built('--via-mir', '--enable-opt', 'constant-fold')
        self.assertEqual(enabled(mir), {'integer_add_zero_right', 'constant-fold'})
        self.assertEqual(mir['optimization']['active_stages'], ['processor', 'lowering', 'mir'])
        mir_unfolded = built('--via-mir', '--disable-opt', 'constant-fold')
        self.assertNotEqual(mir['outputs']['sample'], mir_unfolded['outputs']['sample'])
        release = built('--opt-profile', 'release', '--disable-opt', 'decision-ssa', '--via-mir')
        self.assertNotIn('decision-ssa', enabled(release))
        self.assertIn('constant-fold', enabled(release))
        self.assertEqual(release['cflags'], ['-O0', '-g'])
        self.build('--enable-opt', 'dead-pure', negative='requires a MIR backend')
        self.build('--enable-opt', 'not_a_rule', negative='unknown optimization')
        self.build('--enable-opt', 'all', '--no-optimizations', negative='conflicting optimization')
        self.assertEqual(json.loads((self.output / '.on1x-build.json').read_text()), release)
        self.app(extra=rules + 'backend mir-c11\n', implementation=program)
        self.build('--frozen', negative='stale optimization policy')
        self.lock('--on1xc', OPTIONS.on1xc)
        self.assertEqual(built()['optimization']['backend'], 'mir-c11')
        self.app(extra='optimization "not_a_rule" on\n', implementation=program)
        self.lock('--on1xc', OPTIONS.on1xc, negative='unknown optimization')
        self.app(extra='optimization-profile debug\n', implementation='fn main(){println(missing);}\n')
        self.lock()
        self.build('--no-optimizations', negative='undefined')

    def test_scoped_optimizer_selections(self):
        self.app(extra='backend mir-c11\noptimization-profile debug\n'
                 'optimization "integer_add_zero_right" on\n'
                 'optimization "dev" "integer_multiply_one_right" on\n'
                 'optimization "release" "all" off\n'
                 'optimization "release" "constant-fold" on\n')
        lock = self.lock('--on1xc', OPTIONS.on1xc)
        declared = json.loads(lock.read_text())['optimization']
        self.assertEqual(declared['scoped']['release'], {'all': False, 'constant-fold': True})
        for flags, expected in [
                ([], {'integer_add_zero_right', 'integer_multiply_one_right'}),
                (['--release'], {'constant-fold'}),
                (['--release', '--no-optimizations', '--enable-opt', 'dead-pure'], {'dead-pure'})]:
            output = self.build('--frozen', *flags)
            self.run_bin(output)
            record = json.loads((output / '.on1x-build.json').read_text())['optimization']
            self.assertEqual(record['declared'], declared)
            self.assertEqual({r['name'] for r in record['optimizations'] if r['enabled']}, expected)
        root = self.project / 'On1xfile'
        root.write_text(root.read_text().replace('"release" "constant-fold" on', '"release" "constant-fold" off'))
        self.build('--frozen', negative='stale optimization policy')
        self.app(extra='optimization "undeclared" "all" on\n')
        self.build(negative='unknown optimization scope')
        self.app(extra='optimization "dev" "all" off\noptimization "dev" "all" on\n')
        self.build(negative='duplicate declaration')

    def test_project_backend_scope_lock_and_precedence(self):
        self.app(extra='backend c11\nbackend "dev" mir-vm\nbackend "release" mir-archon\n')
        lock = self.lock()
        self.assertEqual(json.loads(lock.read_text())['optimization']['backend_scopes'],
                         {'dev': 'mir-vm', 'release': 'mir-archon'})
        # CLI can choose a different qualified backend even if the scoped
        # choice has no compiler/runtime installed on this machine.
        out = self.build('--via-mir', '--frozen')
        self.run_bin(out)
        self.assertEqual(json.loads((out / '.on1x-build.json').read_text())['backend']['name'], 'mir-c11')
        manifest = self.project / 'On1xfile'
        manifest.write_text(manifest.read_text().replace('"release" mir-archon', '"release" c11'))
        self.build('--backend', 'c11', '--frozen', negative='stale optimization policy')
        self.app(extra='backend "typo" mir-archon\n')
        self.build(negative='unknown backend scope')
        self.app(extra='backend "dev" c11\nbackend "dev" mir-vm\n')
        self.build(negative='duplicate declaration')
        self.app(extra='backend "dev" nonexistent\n')
        self.build(negative='unknown project backend')

    def test_backend_discovery(self):
        # Probe installed/build tools using fixed trusted input, independently
        # of the current project's manifest and without registering a target.
        self.write('On1xfile', 'this is not a project\n')
        out = subprocess.check_output([sys.executable, OPTIONS.driver, 'backends',
                                      '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc], cwd=self.project)
        result = json.loads(out)
        self.assertEqual(result['schema'], 'on1x.project-backends/v1')
        rows = {r['name']: r for r in result['backends']}
        self.assertEqual(set(rows), {'c11', 'mir-c11', 'mir-archon', 'mir-vm'})
        self.assertTrue(rows['c11']['available'] and rows['mir-c11']['available'])
        for item in rows.values():
            if not item['available']:
                self.assertTrue(item['reason'])
        self.assertFalse((self.project/'target').exists())

    def test_native_project_runner(self):
        self.app(extra='backend "dev" mir-archon\n')
        build = Path(OPTIONS.on1xc).resolve().parent
        archon = build/'backends/mir-archon/on1x-archon-mir'
        if (not archon.is_file() or subprocess.check_output([archon, '--print-target'], text=True).strip()
                != 'x86_64-unknown-linux-gnu'):
            self.build(negative='mir-archon unavailable')
            return
        self.lock()
        out = self.build('--frozen')
        self.run_bin(out)
        report = json.loads((out / '.on1x-build.json').read_text())
        self.assertEqual(report['backend']['artifact'], 'elf-executable')
        self.assertTrue({'archon', 'archon_manifest', 'runtime', 'scalar_abi', 'assembler', 'linker'}
                        <= report['tools'].keys())
        self.assertTrue((out/'sample').read_bytes().startswith(b'\x7fELF'))
        self.build('--frozen')
        self.assertEqual(json.loads((out / '.on1x-build.json').read_text()), report)
        self.write('src/main.on1x', 'fn main(){println("not scalar MIR");}\n')
        self.build('--frozen', negative='MIR')
        self.run_bin(out)

    def test_vm_project_runner(self):
        self.app(extra='backend "dev" mir-vm\n',
                 implementation='import "divide.on1x";fn main(){println(divide(84,2));}\n')
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nlibrary "divide.on1x"\n')
        self.write('src/divide.on1x', 'fn divide(x:i64,d:i64)->i64{return x/d;}\n')
        runner = Path(OPTIONS.on1xc).resolve().with_name('on1x-vm')
        if not runner.is_file():
            self.build(negative='mir-vm unavailable')
            return
        self.lock()
        out = self.build('--frozen')
        report = json.loads((out / '.on1x-build.json').read_text())
        self.assertEqual(report['backend']['artifact'], 'bytecode')
        self.assertFalse(report['cflags'])
        self.assertNotIn('cc', report['tools'])
        for name in ('sample', 'sample.o1vm', '.on1x-vm-runner'):
            self.assertEqual(hashlib.sha256((out / name).read_bytes()).hexdigest(), report['outputs'][name])
        self.assertEqual(report['tools']['vm_runner'], report['outputs']['.on1x-vm-runner'])
        relocated = self.root / 'moved VM generation'
        out.rename(relocated)
        self.run_bin(relocated)
        plan = subprocess.run([sys.executable, OPTIONS.driver, 'plan', self.project, '--frozen',
                               '--on1xc', OPTIONS.on1xc, '--cc', '/no-host-compiler'], text=True,
                              capture_output=True, timeout=60)
        self.assertEqual(plan.returncode, 0, plan.stderr)
        self.assertEqual(json.loads(plan.stdout)['tools'], report['tools'])
        self.build('--frozen')
        self.assertEqual(json.loads((out / '.on1x-build.json').read_text()), report)
        self.write('src/main.on1x', 'import "divide.on1x";fn main(){println(divide(84,0));}\n')
        self.build('--frozen')
        trapped = subprocess.run([out/'sample'], text=True, capture_output=True)
        self.assertEqual(trapped.returncode, 70)
        self.assertIn('/__on1x__/project/src/divide.on1x:1:', trapped.stderr)
        (relocated / 'sample.o1vm').write_bytes(b'not bytecode')
        self.assertNotEqual(subprocess.run([relocated/'sample'], capture_output=True).returncode, 0)

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
        self.write('On1xfile', original.replace(f'"{commit}"', f'commit "{commit}"'))
        self.build()
        self.run_bin()
        self.write('On1xfile', original.replace(commit, 'HEAD'))
        self.build(negative='requires a full lowercase commit')
        self.write('On1xfile', original.replace(commit, '0' * len(commit)))
        self.build(negative='is missing from the offline object store')

    @unittest.skipUnless(shutil.which('git'), 'Git is unavailable')
    def test_git_branch_tag_locking_and_frozen_reproducibility(self):
        author = self.library('author', 'toolkit')
        environment = dict(os.environ, GIT_CONFIG_GLOBAL='/dev/null', GIT_CONFIG_NOSYSTEM='1',
                           GIT_AUTHOR_NAME='Build test', GIT_AUTHOR_EMAIL='build@localhost',
                           GIT_COMMITTER_NAME='Build test', GIT_COMMITTER_EMAIL='build@localhost')

        def git(*arguments, cwd=author):
            return subprocess.run(['git', '-C', cwd, *arguments], check=True, env=environment,
                                  capture_output=True, text=True, timeout=20).stdout.strip()

        git('init', '-q', '--template=', '--initial-branch=main')
        git('add', 'On1xfile', 'lib')
        git('commit', '-qm', 'version 1')
        original = git('rev-parse', 'HEAD')
        bare = self.project / 'vendor/toolkit.git'
        bare.parent.mkdir()
        subprocess.run(['git', 'clone', '--bare', '--quiet', author, bare], check=True,
                       env=environment, capture_output=True, timeout=20)
        self.app(extra='dependency "toolkit" git "vendor/toolkit.git" branch "main"\n',
                 implementation='import toolkit.value as V;\n'
                                'fn main() -> i64 { println(V.number()); return 0; }\n')
        self.build('--frozen', negative='lockfile is required')
        lock = self.lock('--offline')
        document = json.loads(lock.read_text())
        entry = document['dependencies']['toolkit']
        self.assertEqual(entry['selector'], {'kind': 'branch', 'value': 'main'})
        self.assertEqual(entry['commit'], original)
        self.build('--frozen')
        self.run_bin()

        self.write('author/lib/value.on1x', 'fn number() -> i64 { return 99; }\n')
        git('commit', '-qam', 'version 2')
        updated = git('rev-parse', 'HEAD')
        git('push', '--quiet', '--force', str(bare), 'HEAD:refs/heads/main')
        locked_bytes = lock.read_bytes()
        self.build('--frozen')
        self.run_bin()  # The moved branch cannot affect the locked build.
        self.assertEqual(lock.read_bytes(), locked_bytes)
        self.lock('--offline')
        self.assertEqual(json.loads(lock.read_text())['dependencies']['toolkit']['commit'], updated)
        self.build('--frozen')
        self.run_bin(expected='99\n')

        git('tag', '-am', 'release 1', 'release-1', original)
        git('push', '--quiet', str(bare), 'refs/tags/release-1')
        self.write('On1xfile', (self.project / 'On1xfile').read_text().replace(
            'branch "main"', 'tag "release-1"'))
        self.build(negative='stale entry')
        self.lock('--offline')
        entry = json.loads(lock.read_text())['dependencies']['toolkit']
        self.assertEqual(entry['selector'], {'kind': 'tag', 'value': 'release-1'})
        self.assertEqual(entry['commit'], original)  # Annotated tag is peeled to its commit.
        frozen = lock.read_bytes()
        self.lock('--offline')
        self.assertEqual(lock.read_bytes(), frozen)
        self.lock('--frozen')
        self.assertEqual(lock.read_bytes(), frozen)
        self.build('--frozen')
        self.run_bin()

    def test_git_selector_syntax_rejected_before_access(self):
        self.app()
        base = (self.project / 'On1xfile').read_text()
        cases = (
            ('branch', '-config', 'invalid Git branch'),
            ('branch', '../main', 'invalid Git branch'),
            ('branch', 'main..old', 'invalid Git branch'),
            ('tag', 'v1^{commit}', 'invalid Git tag'),
            ('revision', 'main', 'selector must be commit, branch or tag'),
        )
        for selector, value, diagnostic in cases:
            with self.subTest(selector=selector, value=value):
                self.write('On1xfile', base + f'dependency "toolkit" git "missing.git" '
                           f'{selector} {json.dumps(value)}\n')
                self.lock('--offline', negative=diagnostic)

    def test_lockfile_schema_and_symlink_rejected(self):
        commit = '0' * 40
        self.app(extra=f'dependency "toolkit" git "missing.git" "{commit}"\n')
        lock = self.write('On1xfile.lock',
                          '{"schema":"on1x.lock/v1","schema":"on1x.lock/v1","dependencies":{}}\n')
        self.build('--frozen', negative='duplicate lockfile key')
        lock.write_text(json.dumps({
            'schema': 'on1x.lock/v1',
            'dependencies': {'toolkit': {
                'kind': 'git',
                'source': 'missing.git',
                'selector': {'kind': 'commit', 'value': commit},
                'commit': commit,
                'unexpected': True,
            }},
        }))
        self.build('--frozen', negative='expected exactly kind, source, selector and commit')
        lock.unlink()
        lock.symlink_to(self.project / 'On1xfile')
        self.build('--frozen', negative='expected a regular lockfile')

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
