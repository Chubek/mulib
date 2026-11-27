#!/usr/bin/env python3
"""Exercise the real compiler, Git object store, Mune, and QMR when available."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
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

    def test_named_module_fragments_and_interface_on_executable_path(self):
        rules = ('module "Math.Tools" "src/z.on1xm"\n'
                 'module "Math.Tools" "src/a.on1xm"\n'
                 'module-interface "Math.Tools" "src/z-api.on1xi"\n'
                 'module-interface "Math.Tools" "src/a-api.on1xi"\n'
                 'module-interface-policy "Math.Tools" required\n'
                 'module "Base" "src/base-fragment.on1xm"\n')
        source = 'import Math.Tools : answer; fn main(){println(answer());}\n'
        self.app(rules, source)
        self.write('src/z.on1xm', 'module Math.Tools; import Base : base; '
                   'public fn answer()->i64{return base()+21;}\n')
        self.write('src/a.on1xm', 'module Math.Tools; public fn spare()->i64{return 42;} '
                   'public fn hidden()->i64{return 42;}\n')
        self.write('src/base-fragment.on1xm', 'module Base; public fn base()->i64{return 21;}\n')
        self.write('src/z-api.on1xi', 'abstract module Math.Tools; fn answer()->i64;\n')
        self.write('src/a-api.on1xi', 'abstract module Math.Tools; fn spare()->i64;\n')
        output = self.build()
        self.run_bin(output)
        marker = output / '.on1x-build.json'
        before = marker.read_bytes()
        self.assertEqual(json.loads(before)['module_graph'], [
            {'package': 'sample', 'name': 'Base', 'dependencies': []},
            {'package': 'sample', 'name': 'Math.Tools',
             'dependencies': [{'package': 'sample', 'name': 'Base'}]},
        ])
        self.write('src/main.on1x', 'import Math.Tools : hidden; fn main(){println(hidden());}\n')
        self.build(negative="has no export 'hidden'")
        self.assertEqual(marker.read_bytes(), before)
        self.write('src/main.on1x', source)
        self.write('src/a.on1xm', 'module Different; public fn spare()->i64{return 42;}\n')
        self.build(negative='source module header does not match declared membership')
        self.assertEqual(marker.read_bytes(), before)

    def test_program_import_requires_project_opt_in_even_without_modules(self):
        source = 'import program tool : value as get; fn main(){println(get());}\n'
        self.app(implementation=source)
        roles = 'on1xfile 1\nsource "main.on1x"\nlibrary "tool.on1x"\n'
        self.write('src/On1xfile.m', roles)
        self.write('src/tool.on1x', 'fn value()->i64{return 42;} fn main(){println(999);}\n')
        self.build(negative='program is not in the project index: tool')
        self.app('program "tool" "src/tool.on1x"\n', source)
        self.write('src/On1xfile.m', roles)
        self.run_bin(self.build())
        self.write('src/main.on1x', 'import program "tool.on1x" : value as get; fn main(){println(get());}\n')
        self.run_bin(self.build())
        self.write('src/main.on1x', 'import tool; fn main(){println(tool.value());}\n')
        self.build(negative='indexed program requires an explicit program import')

    def test_published_module_package_reloads_scoped_members(self):
        rules = ('module "Outer" "lib/outer.on1xm"\n'
                 'module "Inner" "lib/inner.on1xm"\n')
        self.write('pkg/On1xfile', 'on1xfile 1\nkind library\nname "dep"\ndirectory "lib"\n' + rules)
        self.write('pkg/lib/outer.on1xm', 'module Outer; import Inner : value; '
                   'public fn answer()->i64{return value();}\n')
        self.write('pkg/lib/inner.on1xm', 'module Inner; public fn value()->i64{return 42;}\n')
        published = self.build(root=self.project / 'pkg') / 'library'
        self.assertIn('module "Outer" "lib/outer.on1xm"\n', (published / 'On1xfile').read_text())
        self.assertTrue((published / 'lib/outer.on1xm').is_file())
        self.assertFalse(list(published.glob('.on1x-check-module-*.on1x')))
        self.app('dependency "dep" local "pkg/target/dev/library"\n',
                 'import dep.Outer : answer; fn main(){println(answer());}\n')
        output = self.build()
        self.run_bin(output)
        self.assertEqual(json.loads((output / '.on1x-build.json').read_bytes())['module_graph'], [
            {'package': 'dep', 'name': 'Inner', 'dependencies': []},
            {'package': 'dep', 'name': 'Outer',
             'dependencies': [{'package': 'dep', 'name': 'Inner'}]},
        ])

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

    def test_scrutiny_work_policy(self):
        self.app(extra='scrutiny-work 8192\n')
        manifest = self.project / 'On1xfile'
        formatter = [sys.executable, OPTIONS.driver, 'format', str(manifest)]
        self.assertEqual(subprocess.check_output(formatter, timeout=20), manifest.read_bytes())
        plan = [sys.executable, OPTIONS.driver, 'plan', str(self.project), '--on1xc', OPTIONS.on1xc]
        self.assertEqual(json.loads(subprocess.check_output(plan, timeout=30))['scrutiny_work'], 8192)
        self.assertEqual(json.loads(subprocess.check_output(
            [*plan, '--scrutiny-work', '1'], timeout=30))['scrutiny_work'], 1)
        self.assertFalse(self.output.exists())

        for flags, expected in [((), 8192), (('--via-mir', '--scrutiny-work', '10000000'), 10_000_000)]:
            out = self.build(*flags)
            self.run_bin(out)
            self.assertEqual(json.loads((out / '.on1x-build.json').read_bytes())['scrutiny_work'], expected)

        marker = self.output / '.on1x-build.json'
        previous = marker.read_bytes()
        error = 'scrutiny-work requires a decimal integer from 1 to 10000000'
        for setting in ('0', '-1', '10000001', '9' * 120, 'abc'):
            with self.subTest(setting=setting):
                self.app(extra='scrutiny-work 8192\n')
                self.build('--scrutiny-work', setting, negative=error)
                self.app(extra=f'scrutiny-work {setting}\n')
                self.build(negative=error)
                self.assertEqual(marker.read_bytes(), previous)

        self.app(extra='scrutiny-work "8192"\n')
        self.build(negative='incorrectly quoted fields')
        self.app(extra='scrutiny-work 5\nscrutiny-work 6\n')
        self.build(negative='duplicate declaration scrutiny-work')
        self.app(extra='dependency "dep" local "dep"\n')
        self.library('dep', 'dep')
        dep = self.project / 'dep/On1xfile'
        dep.write_text(dep.read_text() + 'scrutiny-work 4\n')
        self.build(negative="unsupported 'scrutiny-work' in dependency On1xfile")

    def test_scrutiny_work_reaches_analysis(self):
        self.app(extra='scrutiny strict\nscrutiny-work 1\n',
                 implementation='fn main()->i64{return 42/0;}\n')
        probe = subprocess.run(
            [OPTIONS.on1xc, '--no-prelude', '--check', '--scrutiny', 'strict', self.project/'src/main.on1x'],
            text=True, capture_output=True, timeout=20)
        if probe.returncode == 0:
            self.skipTest('Wetstone analysis is unavailable in this build')
        self.assertIn('ON1X-ANALYSIS-DIVZERO', probe.stderr)
        for flags in [(), ('--via-mir',)]:
            out = self.build(*flags)
            marker = out / '.on1x-build.json'
            before = marker.read_bytes()
            self.assertEqual(json.loads(before)['scrutiny_work'], 1)
            self.build(*flags, '--scrutiny-work', '8192', negative='ON1X-ANALYSIS-DIVZERO')
            self.assertEqual(marker.read_bytes(), before)

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

    def test_c11_memgraph_policy(self):
        listing = json.loads(subprocess.check_output(
            [OPTIONS.on1xc, '--list-optimizations'], text=True, timeout=30))
        if not any(row['name'] == 'memgraph-bounds' for row in listing['optimizations']):
            self.skipTest('compiler built without LibStone LLSynth MEMGraph')
        self.app(extra='optimization "memgraph-bounds" on\n',
                 implementation='fn main(){let a=array[1,42];println(a[1]);}\n')
        out = self.build()
        self.run_bin(out)
        policy = json.loads((out / '.on1x-build.json').read_text())['optimization']
        self.assertEqual(policy['active_stages'], ['processor', 'hir'])
        self.assertIn({'name': 'memgraph-bounds', 'stage': 'hir', 'enabled': True},
                      policy['optimizations'])
        self.build('--via-mir', negative='optimization memgraph-bounds requires the C11 or mir-vm backend')
        out = self.build('--backend', 'mir-vm')
        self.run_bin(out)
        vm_policy = json.loads((out / '.on1x-build.json').read_text())['optimization']
        self.assertEqual(vm_policy['active_stages'], ['processor', 'hir', 'lowering', 'mir'])

    def test_novostone_hir_manifest(self):
        listing = json.loads(subprocess.check_output(
            [OPTIONS.on1xc, '--list-optimizations'], text=True, timeout=30))
        self.app(extra='optimization-profile debug\noptimization "novostone-hir" on\n',
                 implementation='fn main(){println(6*7);}\n')
        if not any(row['name'] == 'novostone-hir' for row in listing['optimizations']):
            self.build(negative='novostone-hir')
            return
        lock = self.lock('--on1xc', OPTIONS.on1xc)
        self.assertEqual(json.loads(lock.read_bytes())['optimization']['selections'], {'novostone-hir': True})
        outputs = []
        for flags in [(), ('--disable-opt', 'novostone-hir')]:
            out = self.build('--frozen', *flags)
            self.run_bin(out)
            report = json.loads((out / '.on1x-build.json').read_bytes())
            rule = next(row for row in report['optimization']['optimizations'] if row['name'] == 'novostone-hir')
            self.assertEqual(rule, {'name': 'novostone-hir', 'stage': 'processor', 'enabled': not flags})
            outputs.append(report['outputs']['sample'])
        self.assertNotEqual(*outputs)

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
        self.assertEqual(set(rows), {'c11', 'mir-c11', 'mir-archon', 'mir-vm', 'archvirt'})
        self.assertEqual(rows['archvirt']['available'], OPTIONS.archvirt_enabled)
        self.assertEqual(rows['archvirt']['root_policy'], ['always', 'opt-in'])
        self.assertTrue(rows['c11']['available'] and rows['mir-c11']['available'])
        for item in rows.values():
            if not item['available']:
                self.assertTrue(item['reason'])
        self.assertFalse((self.project/'target').exists())

    def project_command(self, action, *arguments, compiler=None, env=None):
        return subprocess.run(
            [sys.executable, OPTIONS.driver, action, '--on1xc', compiler or OPTIONS.on1xc,
             str(self.project), *arguments], env=env, text=True, capture_output=True, timeout=180)

    def test_archvirt_root_only(self):
        self.app()
        rejected = 'vm policy is only allowed in the root On1xfile'
        for path in ('src/On1xfile.m', 'src/On1xfile.i', 'src/nested/On1xfile'):
            original = self.project / path
            text = original.read_text() if original.is_file() else 'on1xfile 1\n'
            self.write(path, text + '  vm\\ \n always # must not apply\n')
            for action in ('plan', 'build'):
                with self.subTest(path=path, action=action):
                    result = self.project_command(action)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn(rejected, result.stderr)
                    self.assertIn(path, result.stderr)
            self.write(path, text)

        self.app(extra='dependency "toolkit" local "vendor/toolkit"\n')
        dependency = self.library('vendor/toolkit', 'toolkit')
        manifest = dependency / 'On1xfile'
        manifest.write_text(manifest.read_text() + 'vm\n opt-in\n')
        self.build(negative=rejected)
        self.build(root=dependency, negative=rejected)
        # Even direct compiler package discovery rejects the directive and has
        # no ability to turn a package import into a VM launch.
        source = self.write('probe.on1x', 'import toolkit.value;\nfn main()->i64{return 0;}\n')
        discovered = subprocess.run([OPTIONS.on1xc, '--check', source, '-I', dependency.parent],
                                    text=True, capture_output=True, timeout=20)
        self.assertNotEqual(discovered.returncode, 0, discovered.stdout)
        self.assertIn(rejected, discovered.stderr)
        self.assertFalse(self.output.exists())

    def test_archvirt_locked_policy(self):
        for extra, diagnostic in [
                ('vm disabled\n', 'vm policy must be opt-in or always'),
                ('vm always\nvm opt-in\n', 'duplicate declaration vm'),
                ('vm "always"\n', 'incorrectly quoted fields'),
                ('vm opt-in\nbackend "dev" c11\n', 'cannot be combined with hosted backends'),
                ('vm opt-in\nkernel "sha"\n', 'cannot be combined with hosted backends')]:
            self.app(extra=extra)
            result = self.project_command('plan')
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(diagnostic, result.stderr)
        self.app(extra='vm always\noptimization-profile debug\n')

        for action in ('plan', 'build', 'run'):
            for flag in ('--locked', '--frozen'):
                result = self.project_command(action, flag)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn('lockfile is required', result.stderr)
        path = self.lock()
        saved = path.read_bytes()
        self.assertEqual(json.loads(saved), {'schema': 'on1x.lock/v3', 'dependencies': {},
                                              'optimization': {'optimization-profile': 'debug'}, 'vm': 'always'})
        plan = self.project_command('plan', '--frozen')
        self.assertEqual(plan.returncode, 0, plan.stderr)
        self.assertEqual(json.loads(plan.stdout)['backend'], {
            'name': 'archvirt', 'artifact': 'archvirt-launcher', 'artifacts': ['sample'], 'vm': 'always'})
        for options in (('--backend', 'c11'), ('--via-mir',), ('--cc', '/usr/bin/cc'), ('--optimize-ir',)):
            result = self.project_command('plan', *options)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn('ArchVirt projects do not accept', result.stderr)
        for extra in ('vm opt-in\noptimization-profile debug\n', 'optimization-profile debug\n'):
            self.app(extra=extra)
            result = self.project_command('build', '--frozen')
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn('stale vm policy', result.stderr)
        self.assertEqual(path.read_bytes(), saved)
        self.assertFalse(self.output.exists())

    def test_archvirt_build_run_dispatch(self):
        # Test the process contract independently of the optional guest tools.
        # Leave all source/stdlib/analysis checks with the real compiler.
        log = self.root / 'vm dispatch.jsonl'
        compiler = self.root / 'fixture compiler'
        trusted = self.root / 'trusted toolchain files'
        trusted.mkdir()
        tool = trusted / 'guest-tool'
        tool.write_text('expected tool input')
        (trusted / 'not mounted').touch()
        alias = trusted / 'guest tool alias'
        alias.symlink_to(tool.name)
        compiler.write_text(f'''#!/usr/bin/python3
import json, os, subprocess, sys
from pathlib import Path
args = sys.argv[1:]
record = Path({str(log)!r})
if args[0] == '--print-archvirt-build-inputs':
    print({str(OPTIONS.on1xc)!r}, {str(alias)!r}, sep='\\n')
    inputs = subprocess.run([{str(OPTIONS.on1xc)!r}, args[0]], capture_output=True, text=True)
    if inputs.returncode == 0:
        print(inputs.stdout, end='')
elif args[0].startswith('--vm-launcher=') or args[0] == '--vm-build':
    assert 'XDG_CACHE_HOME' not in os.environ and os.environ['HOME'] == '/nonexistent'
    if Path('/work/input/project').is_dir():
        assert Path({str(alias)!r}).read_text() == 'expected tool input'
        assert not Path({str(tool)!r}).exists() and not Path({str(trusted / 'not mounted')!r}).exists()
    else:
        with record.open('a') as stream:
            stream.write(json.dumps(args) + '\\n')
    if 'FAIL_GUEST_BUILD' in Path(args[1]).read_text():
        sys.exit('guest build unavailable')
    output = Path(args[args.index('-o') + 1])
    if args[0] == '--vm-build':
        output.write_bytes(b'fixture guest image')
    else:
        output.write_text('#!/usr/bin/python3\\nraise SystemExit("unexpected launcher execution")\\n')
        output.chmod(0o755)
elif args[0] == '--vm-run-elf':
    assert len(args) == 2 and Path(args[1]).read_bytes() == b'fixture guest image'
    assert not Path('/work/input/project').is_dir()
    assert 'DOMVIRT_KERNEL' not in os.environ
    with record.open('a') as stream:
        stream.write(json.dumps([*args, os.environ.get('XDG_CACHE_HOME'), os.environ['HOME']]) + '\\n')
    print('guest executed')
    print('ArchVirt cache key: ' + 'a' * 64, file=sys.stderr)
    sys.exit(47)
else:
    os.execv({str(OPTIONS.on1xc)!r}, [{str(OPTIONS.on1xc)!r}, *args])
''')
        compiler.chmod(0o755)

        def records():
            return [json.loads(line) for line in log.read_text().splitlines()]

        self.app()
        result = self.project_command('run', compiler=compiler)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('no host execution fallback', result.stderr)
        self.assertFalse(log.exists())
        env = dict(os.environ, XDG_CACHE_HOME=str(self.root / 'user cache'),
                   HOME=str(self.root / 'user home'), DOMVIRT_KERNEL='/ambient/untrusted')
        for policy in ('opt-in', 'always'):
            self.app(extra=f'vm {policy}\n')
            self.lock()
            log.unlink(missing_ok=True)
            planned = self.project_command('plan', '--frozen', compiler=compiler)
            self.assertEqual(planned.returncode, 0, planned.stderr)
            self.assertFalse(log.exists(), planned.stdout)
            built = self.project_command('build', '--frozen', compiler=compiler, env=env)
            self.assertEqual(built.returncode, 0, built.stderr)
            self.assertEqual(len(records()), 1, records())
            call = records()[0]
            self.assertEqual(call[0], f'--vm-launcher={policy}')
            self.assertIn('--stdlib', call)
            self.assertEqual(call.count('-I'), 2)
            self.assertTrue(call[1].endswith('/input/project/src/main.on1x'), call)
            self.assertEqual(set(json.loads((self.output / '.on1x-build.json').read_text())['outputs']),
                             {'sample'})
            before = (self.output / '.on1x-build.json').read_bytes()

            # Neither option-like, spaced nor shell-like guest args are
            # consumed as project flags or dropped silently.
            rejected = self.project_command('run', '--frozen', '--', '--sandbox', 'a b',
                                            '$(echo not-a-shell)', compiler=compiler)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn('does not support guest arguments', rejected.stderr)
            self.assertEqual(len(records()), 1, records())
            invoked = self.project_command('run', '--frozen', '--', compiler=compiler, env=env)
            self.assertEqual(invoked.returncode, 47, invoked.stderr)
            self.assertEqual(invoked.stdout, 'guest executed\n')
            self.assertIn('ArchVirt cache key: ' + 'a' * 64, invoked.stderr)
            compiled, launched = records()[1:]
            self.assertEqual(compiled[0], '--vm-build')
            self.assertEqual(launched, ['--vm-run-elf', compiled[compiled.index('-o') + 1],
                                        env['XDG_CACHE_HOME'], env['HOME']])
            self.assertEqual((self.output / '.on1x-build.json').read_bytes(), before)

        if self.bwrap_available():
            relative_env = dict(env, XDG_CACHE_HOME='relative-cache', HOME='relative-home')
            isolated = self.project_command('run', '--frozen', '--sandbox', compiler=compiler, env=relative_env)
            self.assertEqual(isolated.returncode, 47, isolated.stderr)
            self.assertEqual(isolated.stdout, 'guest executed\n')
            self.assertEqual(records()[-1][0], '--vm-run-elf')
            self.assertEqual(records()[-1][2:], [None, '/nonexistent'])
            alias.unlink()
            alias.mkdir()
            refused = self.project_command('build', '--frozen', '--sandbox', compiler=compiler)
            self.assertNotEqual(refused.returncode, 0, refused.stdout)
            self.assertIn('invalid regular file in ArchVirt build toolchain', refused.stderr)
            self.assertEqual((self.output / '.on1x-build.json').read_bytes(), before)
            alias.rmdir()
            alias.symlink_to(tool.name)

        self.app(extra='vm always\n', implementation='fn FAIL_GUEST_BUILD()->i64{return 0;}\n'
                                                    'fn main()->i64{return FAIL_GUEST_BUILD();}\n')
        for action in ('build', 'run'):
            count = len(records())
            failed = self.project_command(action, '--frozen', compiler=compiler)
            self.assertNotEqual(failed.returncode, 0, failed.stdout)
            self.assertIn('guest build unavailable', failed.stderr)
            self.assertEqual(len(records()), count + 1, records())
            self.assertEqual((self.output / '.on1x-build.json').read_bytes(), before)

        self.app(extra='vm always\ntarget "second" "src/second.on1x"\n')
        root = self.project / 'On1xfile'
        root.write_text(root.read_text().replace('kind executable', 'kind complex'))
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nsource "second.on1x"\n')
        self.write('src/second.on1x', 'fn main()->i64{return 0;}\n')
        count = len(records())
        missing = self.project_command('run', '--frozen', compiler=compiler)
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn('select it with --target', missing.stderr)
        self.assertEqual(len(records()), count)
        selected = self.project_command('run', '--frozen', '--target', 'second', compiler=compiler)
        self.assertEqual(selected.returncode, 47, selected.stderr)
        self.assertTrue(records()[-2][1].endswith('/input/project/src/second.on1x'))

    def test_archvirt_cache_commands(self):
        key = 'b' * 64
        log = self.root / 'calls.jsonl'
        compiler = self.root / 'fixture cache driver'
        compiler.write_text(f'''#!/usr/bin/python3
import json, os, sys
assert sys.argv[1:] in (['--vm-rerun', {key!r}], ['--vm-cache-clear'])
assert 'ON1X_LIB_PATH' not in os.environ
with open({str(log)!r}, 'a') as stream:
    stream.write(json.dumps([*sys.argv[1:], os.environ.get('XDG_CACHE_HOME')]) + '\\n')
sys.exit(37 if sys.argv[1] == '--vm-rerun' else 0)
''')
        compiler.chmod(0o755)
        env = dict(os.environ, XDG_CACHE_HOME=str(self.root / 'caller cache'), ON1X_LIB_PATH='/irrelevant')

        def call(action, *options):
            return self.project_command(action, *([key] if action == 'vm-rerun' else []),
                                        *options, compiler=compiler, env=env)

        self.app()
        actions = ('vm-rerun', 'vm-cache-clear')
        for action in actions:
            self.assertIn('requires vm opt-in or vm always', call(action).stderr)
        self.app(extra='vm always\ndependency "toolkit" local "vendor/toolkit"\n')
        self.write('vendor/toolkit/On1xfile', 'on1xfile 1\nkind library\nname "toolkit"\nvm always\n')
        (self.project / 'src/main.on1x').unlink()
        for action in actions:
            self.assertIn('lockfile is required', call(action, '--frozen').stderr)
        locked = self.lock()
        original = locked.read_bytes()
        result = self.project_command('vm-rerun', '../escape', compiler=compiler, env=env)
        self.assertIn('requires a lowercase 64-hex cache key', result.stderr)
        self.assertFalse(log.exists())
        self.assertEqual(call('vm-rerun', '--frozen').returncode, 37)
        self.assertEqual(call('vm-cache-clear', '--locked').returncode, 0)
        self.assertEqual([json.loads(line) for line in log.read_text().splitlines()],
                         [['--vm-rerun', key, env['XDG_CACHE_HOME']], ['--vm-cache-clear', env['XDG_CACHE_HOME']]])
        self.app(extra='vm opt-in\ndependency "toolkit" local "vendor/toolkit"\n')
        for action in actions:
            self.assertIn('stale vm policy', call(action).stderr)
        self.assertEqual(len(log.read_text().splitlines()), 2)
        self.assertEqual(locked.read_bytes(), original)
        self.assertFalse(self.output.exists())

    def test_archvirt_real_compiler(self):
        if not OPTIONS.archvirt_enabled:
            self.skipTest('ArchVirt integration not selected')
        source = ('import toolkit.value as V;\n'
                  'fn item(i:i64)->i64{let a=array[42,0];return a[i];}\n'
                  'fn main()->i64{println(item(V.number()-42));return 37;}\n')
        for policy in ('always', 'opt-in'):
            with self.subTest(policy=policy):
                self.library('vendor/toolkit', 'toolkit')
                cache = self.root / ('cache ' + policy)
                env = dict(os.environ, XDG_CACHE_HOME=str(cache), HOME=str(self.root))
                self.app(extra=f'vm {policy}\ndependency "toolkit" local "vendor/toolkit"\n',
                         implementation=source)
                self.lock()
                # The installed guest runner is required for this enabled
                # test. A failed VM setup is not treated as a passing result.
                built = self.project_command('build', '--frozen', env=env)
                self.assertEqual(built.returncode, 0, built.stderr)
                self.assertEqual(built.stdout.strip(), str(self.output))
                launcher = self.output / 'sample'
                binary = launcher.read_bytes()
                self.assertEqual(binary[:4], b'\x7fELF')
                self.assertIn(b'/__on1x__/project/src/main.on1x', binary)
                rebuilt = self.project_command('build', '--frozen', env=env)
                self.assertEqual(rebuilt.returncode, 0, rebuilt.stderr)
                self.assertEqual(launcher.read_bytes(), binary)
                self.assertFalse(cache.exists(), 'build must not populate the replay cache')
                if policy == 'opt-in':
                    refused = subprocess.run([launcher], capture_output=True, text=True, timeout=30)
                    self.assertEqual(refused.returncode, 2, refused.stderr)
                    self.assertIn('no native payload', refused.stderr)
                    self.assertEqual(refused.stdout, '')
                launch = [launcher] if policy == 'always' else [launcher, '--sandbox']
                direct = subprocess.run(launch, env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(direct.returncode, 37, direct.stderr)
                self.assertEqual(direct.stdout, '42\n')
                self.assertFalse(cache.exists(), 'linked launchers do not populate the replay cache')

                prepared = self.project_command('run', '--prepare', '--frozen', env=env)
                self.assertEqual(prepared.returncode, 0, prepared.stderr)
                self.assertEqual(prepared.stdout, '')
                registered = re.search(r'ArchVirt VM ID: ([0-9a-f]{64})\b', prepared.stderr)
                self.assertIsNotNone(registered, prepared.stderr)
                ran = self.project_command('run', '--frozen', env=env)
                self.assertEqual(ran.returncode, 37, ran.stderr)
                self.assertEqual(ran.stdout, '42\n')
                match = re.search(r'ArchVirt cache key: ([0-9a-f]{64})\b', ran.stderr)
                self.assertIsNotNone(match, ran.stderr)
                key = match[1]
                self.assertEqual(registered[1], key)
                if self.bwrap_available():
                    sandboxed = self.project_command('run', '--frozen', '--sandbox', env=env)
                    self.assertEqual(sandboxed.returncode, 37, sandboxed.stderr)
                    self.assertEqual(sandboxed.stdout, '42\n')
                    self.assertIn(key, sandboxed.stderr)
                self.write('vendor/toolkit/On1xfile', 'this dependency is no longer buildable\n')
                self.write('src/main.on1x', 'this source is no longer buildable\n')
                replay = self.project_command('vm-rerun', key, '--frozen', env=env)
                self.assertEqual(replay.returncode, 37, replay.stderr)
                self.assertEqual(replay.stdout, '42\n')
                alternate = dict(env, XDG_CACHE_HOME=str(self.root / 'other user cache'))
                miss = self.project_command('vm-rerun', key, '--frozen', env=alternate)
                self.assertNotEqual(miss.returncode, 37, miss.stderr)
                self.assertEqual(miss.stdout, '')
                unrelated = cache / 'unrelated'
                unrelated.write_text('keep')
                cleared = self.project_command('vm-cache-clear', '--frozen', env=env)
                self.assertEqual(cleared.returncode, 0, cleared.stderr)
                self.assertEqual(unrelated.read_text(), 'keep')
                miss = self.project_command('vm-rerun', key, '--frozen', env=env)
                self.assertNotEqual(miss.returncode, 37, miss.stderr)
                self.assertEqual(miss.stdout, '')
                direct = subprocess.run(launch, env=env, capture_output=True, text=True, timeout=30)
                self.assertEqual(direct.returncode, 37, direct.stderr)
                self.assertEqual(direct.stdout, '42\n')

    @staticmethod
    def bwrap_available():
        bwrap = shutil.which('bwrap')
        return bwrap and subprocess.run([bwrap, '--unshare-all', '--ro-bind', '/usr', '/usr',
                                        '--ro-bind', '/lib64', '/lib64', '/usr/bin/true'],
                                       capture_output=True, timeout=5).returncode == 0

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

    def test_source_library_reports_published_files(self):
        library = self.library('toolkit', 'toolkit')
        self.write('toolkit/lib/On1xfile.m', 'on1xfile 1\n# local note\nlibrary "value.on1x"\n')
        self.write('toolkit/lib/ignored.on1x', 'not declared, not published\n')
        relocated = self.root / 'relocated'
        shutil.copytree(library, relocated)

        output = self.build(root=library)
        marker = output / '.on1x-build.json'
        original_report = marker.read_bytes()
        report = json.loads(original_report)
        expected_paths = {'library/On1xfile', 'library/lib/On1xfile.m', 'library/lib/On1xfile.i',
                          'library/lib/value.on1x', 'library/lib/value.on1xi'}
        self.assertEqual({path.relative_to(output).as_posix() for path in (output / 'library').rglob('*')
                          if path.is_file()}, expected_paths)
        expected = {path: hashlib.sha256((output / path).read_bytes()).hexdigest()
                    for path in expected_paths}
        self.assertEqual(report['outputs'], expected)
        self.assertNotEqual(report['inputs']['project/lib/On1xfile.m'],
                            expected['library/lib/On1xfile.m'])
        repeated = self.build(root=relocated)
        self.assertEqual((repeated / marker.name).read_bytes(), original_report)
        self.assertEqual(self.build(root=library), output)  # Replace a prior generation.
        self.assertEqual(marker.read_bytes(), original_report)

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
        self.app(extra=f'dependency "toolkit" git\n "vendor/repository"\n "{commit}"\n',
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
        self.app(extra='dependency "toolkit"\n git "vendor/toolkit.git"\n branch "main"\n',
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

    @unittest.skipUnless(shutil.which('git'), 'Git is unavailable')
    def test_offline_lock_survives_packing_unrelated_unicode_refs(self):
        repository = self.library('vendor/toolkit', 'toolkit')
        environment = dict(os.environ, GIT_CONFIG_GLOBAL='/dev/null', GIT_CONFIG_NOSYSTEM='1',
                           GIT_AUTHOR_NAME='Build test', GIT_AUTHOR_EMAIL='build@localhost',
                           GIT_COMMITTER_NAME='Build test', GIT_COMMITTER_EMAIL='build@localhost')

        def git(*args):
            return subprocess.check_output(['git', '-C', repository, *args], env=environment,
                                           text=True, timeout=20).strip()

        git('init', '-q', '--template=', '--initial-branch=main')
        git('add', '.')
        git('commit', '-qm', 'library')
        git('branch', 'café')
        self.app(extra='dependency "toolkit" git "vendor/toolkit" branch "main"\n')
        lock = self.lock('--offline')
        original = lock.read_bytes()
        self.assertEqual(json.loads(original)['dependencies']['toolkit']['commit'],
                         git('rev-parse', 'HEAD'))
        git('pack-refs', '--all', '--prune')
        self.lock('--offline')
        self.assertEqual(lock.read_bytes(), original)
        git('branch', '-m', 'main', 'next')
        self.lock('--frozen')
        self.lock('--offline', negative="Git branch 'main' was not found")
        self.assertEqual(lock.read_bytes(), original)

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
        self.app(extra='profile "small" "profile.lua"\nprofile-arg "small" private boolean true\n'
                 'define entry "main.on1x"\n')
        self.write('profile.lua', 'assert(io == nil and os == nil and package == nil and math.random == nil)\n'
                   'assert(on1xfile.argument("private") == true)\n'
                   'return on1xfile.profile { optimize = 1, debug = true }\n')
        self.write('src/On1xfile.m', 'on1xfile 1\n'
                   '{% if data.profile == "small" and data.private == nil and on1xfile == nil then %}'
                   'source\n{{ data.entry }}\n{% end %}')
        self.write('src/On1xfile.i', 'on1xfile 1\n{# compiled by real QMR, evaluated by Mune #}\n'
                   'interface\n{{ "main.on1xi" }}\n')
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

    def test_typed_profile_arguments_and_context(self):
        if not OPTIONS.mune_runner:
            self.skipTest('requires MuLIB Mune helper')
        self.app(extra='''
profile-arg "tuned" level integer 2
profile-arg "tuned" debug boolean false
profile-arg "tuned" mode string "ffi"
profile-arg "other" level integer 3
profile-arg "other" isolated boolean true
profile "tuned" "profile.lua"
profile "other" "not-run.lua"
backend "tuned" mir-c11
optimization-profile default
define mode "legacy"
''')
        manifest = self.project / 'On1xfile'
        manifest.write_text(manifest.read_text().replace(
            'kind executable', 'kind complex\ntarget "second" "src/second.on1x"'))
        self.write('src/On1xfile.m', 'on1xfile 1\nsource "main.on1x"\nsource "second.on1x"\n')
        self.write('src/second.on1x', 'fn main() -> i64 { return 0; }\n')
        script = self.write('profile.lua', '''
local c = on1xfile.context()
assert(c.profile == "tuned" and c.project == "sample" and c.kind == "complex")
assert(c.directory == "src" and c.backend == "mir-c11")
assert(c.compiler == "host" and c.assembler == "host" and c.linker == "host")
assert(c.targets.sample == "src/main.on1x")
if c.target == nil then
    assert(c.targets.second == "src/second.on1x")
else
    assert(c.target == "sample" and c.targets.second == nil)
end
assert(os == nil and io == nil and package == nil and math.random == nil)
assert(data.mode == "legacy" and data.level == nil)
assert(on1xfile.argument("mode") == "ffi" and on1xfile.argument("isolated") == nil)
assert(on1xfile.argument("debug") == false and math.type(on1xfile.argument("level")) == "integer")
data.project = "tampered"
c.project = "tampered"
c.targets.sample = "tampered"
local fresh = on1xfile.context()
assert(fresh.project == "sample" and fresh.targets.sample == "src/main.on1x")
return on1xfile.profile {
    optimize = on1xfile.argument("level"), debug = on1xfile.argument("debug")
}
''')
        self.lock('--on1xc', OPTIONS.on1xc)
        plan = [sys.executable, OPTIONS.driver, 'plan', str(self.project),
                '--on1xc', OPTIONS.on1xc, '--cc', OPTIONS.cc,
                '--mune-runner', OPTIONS.mune_runner, '--frozen']
        default = json.loads(subprocess.check_output(plan, timeout=30))
        self.assertNotIn('project/profile.lua', default['inputs'])
        selected = [*plan, '--profile', 'tuned', '--target', 'sample']
        first = subprocess.check_output(selected, timeout=30)
        self.assertEqual(first, subprocess.check_output(selected, timeout=30))
        report = json.loads(first)
        self.assertEqual(report['cflags'], ['-O2'])
        self.assertEqual(report['targets'], {'sample': 'src/main.on1x'})
        self.assertEqual(report['inputs']['project/On1xfile'],
                         hashlib.sha256(manifest.read_bytes()).hexdigest())
        self.assertEqual(report['inputs']['project/profile.lua'],
                         hashlib.sha256(script.read_bytes()).hexdigest())
        self.assertNotIn('project/not-run.lua', report['inputs'])
        all_targets = json.loads(subprocess.check_output([*plan, '--project-command', 'tuned'], timeout=30))
        self.assertEqual(set(all_targets['targets']), {'sample', 'second'})
        output = self.build('--frozen', '--profile', 'tuned', '--target', 'sample')
        self.run_bin(output)
        self.assertEqual(json.loads((output / '.on1x-build.json').read_bytes())['cflags'], ['-O2'])
        manifest.write_text(manifest.read_text().replace('optimization-profile default',
                                                         'optimization-profile release'))
        script.write_text('error("profile should not run against a stale lock")\n')
        self.build('--frozen', '--profile', 'tuned', negative='stale optimization policy')

    def test_profile_argument_manifest_validation(self):
        cases = [
            ('profile-arg "dev" mode string "x"\n', 'requires a declared Lua profile'),
            ('profile-arg "missing" mode string "x"\n', 'requires a declared Lua profile'),
            ('profile-arg "tuned" mode string "a"\nprofile-arg "tuned" mode string "b"\n',
             'duplicate declaration mode'),
            ('profile-arg "tuned" mode boolean yes\n', 'boolean must be true or false'),
            ('profile-arg "tuned" mode boolean "true"\n', 'incorrectly quoted'),
            ('profile-arg "tuned" mode integer 9223372036854775808\n', 'signed 64-bit'),
            ('profile-arg "tuned" mode integer 02\n', 'signed 64-bit'),
            ('profile-arg "tuned" mode number 1\n', 'type must be string, integer or boolean'),
            ('profile-arg "tuned" wrong-key integer 1\n', 'invalid name'),
            ('profile-arg "tuned" mode string unquoted\n', 'incorrectly quoted'),
            (''.join(f'profile-arg "tuned" key{i} boolean true\n' for i in range(65)),
             'declaration count budget exceeded'),
        ]
        for declaration, error in cases:
            with self.subTest(error=error, declaration=declaration[:100]):
                self.app(extra='profile "tuned" "do-not-run.lua"\n' + declaration)
                result = subprocess.run(
                    [sys.executable, OPTIONS.driver, 'format', self.project / 'On1xfile'],
                    text=True, capture_output=True, timeout=30)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(error, result.stderr)

        self.app(extra='profile-arg "tuned" bound integer 9223372036854775807\n'
                 'profile "tuned" "do-not-run.lua"\n')
        subprocess.run([sys.executable, OPTIONS.driver, 'format', self.project / 'On1xfile', '--check'],
                       check=True, timeout=30)

        self.app(extra='dependency "toolkit" local "vendor/toolkit"\n')
        dep = self.library('vendor/toolkit', 'toolkit') / 'On1xfile'
        dep.write_text(dep.read_text() + 'profile-arg "tuned" x boolean true\n')
        self.build(negative='unsupported \'profile-arg\' in dependency On1xfile')

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

    def test_multiline_manifest_format(self):
        path = self.project / 'On1xfile'
        path.write_bytes(
            b' on1xfile\r\n\t1\r\n kind executable\r\n name\r\n "sample"\r\n'
            b' target "sample" # target comment \\\r\n "main.on1x"\r\n'
            b' dependency "mathlib" git\r\n "vendor/mathlib.git" # local repository\r\n'
            b' "0123456789abcdef0123456789abcdef01234567"\r\n'
            b' backend mir-c11\r\n define\\ \t\r\n # blank and comment lines do not consume it\r\n'
            b'\r\n name "literal # ends in a backslash: \\\\"\r\n define other\r\n "backend"\r\n')
        cli = [sys.executable, OPTIONS.driver, 'format', path]
        expected = (
            'on1xfile\n  1\nkind executable\nname\n  "sample"\n'
            'target "sample" # target comment \\\n  "main.on1x"\n'
            'dependency "mathlib" git\n  "vendor/mathlib.git" # local repository\n'
            '  "0123456789abcdef0123456789abcdef01234567"\n'
            'backend mir-c11\ndefine \\\n# blank and comment lines do not consume it\n'
            '\n  name "literal # ends in a backslash: \\\\"\ndefine other\n  "backend"\n')
        self.assertIn('not canonically formatted',
                      subprocess.run([*cli, '--check'], capture_output=True, text=True).stderr)
        self.assertEqual(subprocess.check_output(cli, text=True), expected)
        path.write_text(expected)
        self.assertEqual(subprocess.check_output(cli, text=True), expected)
        subprocess.run([*cli, '--check'], check=True)

        # 'library' is also a role head. A marker can keep this bare argument
        # attached to kind without treating it as a directory-local directive.
        path.write_text('on1xfile 1\nkind \\\nlibrary\nname "sample"\n')
        self.assertIn('kind \\\n  library\n', subprocess.check_output(cli, text=True))

    def test_multiline_manifest_diagnostics(self):
        path = self.write('On1xfile', '')
        cli = [sys.executable, OPTIONS.driver, 'format', path]
        base = 'on1xfile 1\nkind library\nname "sample"\n'
        for text, span, message in (
                ('directory\n # inside row\n "src"\n typo "other"\n # outside span\n',
                 '4-7', 'known unquoted head'),
                ('directory\n "src" name "other"\n', '4-5', 'known unquoted head'),
                ('define\n name "value"\n', '4', 'directive-looking bare field'),
                ('directory\n "src" \\ # not a marker\n', '4-5', 'followed only by whitespace'),
                ('directory\n "unclosed\\\nname "sample"\n', '4-5', 'unterminated quoted string'),
                ('directory "src" \\', '4', 'dangling continuation'),
                ('directory\n "src"\\ \t\r\n\n# still dangling\n', '4-5', 'dangling continuation'),
                ('directory \\\n name "sample"\n', '4-5', 'incorrectly quoted fields'),
                ('\\\n', '3-4', 'must follow a field')):
            with self.subTest(text=text):
                path.write_bytes((base + text).encode())
                result = subprocess.run(cli, stdin=subprocess.DEVNULL, text=True, capture_output=True, timeout=5)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(result.stdout)
                self.assertIn(f'{path}:{span}:', result.stderr)
                self.assertIn(message, result.stderr)

    def test_multiline_manifest_build_and_lock(self):
        self.app()
        manifest = self.write('On1xfile', 'on1xfile 1\nkind\n executable\nname\n "sample"\n'
                              'directory\n "src"\n target\n "sample"\n "src/main.on1x"\nbackend\n c11\n')
        self.write('src/On1xfile.m', 'on1xfile 1\nsource\n "main.on1x"\n')
        interface = self.write('src/On1xfile.i', 'on1xfile 1\ninterface\n "main.on1xi"\n')
        self.write('src/main.on1xi', 'on1xi 1;\nfn main() -> i64;\n')
        self.write('src/inert/On1xfile', 'on1xfile 1\nkind library\nname "inert"\n'
                   'define \\\n vm "not a VM policy"\ndefine \\\n compiler "not a native tool directive"\n')
        lock = self.lock()
        lock_data = lock.read_bytes()
        self.build('--frozen')
        self.run_bin()
        report = self.output / '.on1x-build.json'
        published = report.read_bytes()
        manifest.write_text(manifest.read_text() + 'define\n # not part of the span end\n vm always\n')
        self.lock(negative='On1xfile:13: missing, extra or incorrectly quoted fields')
        self.build(negative='On1xfile:13: missing, extra or incorrectly quoted fields')
        self.assertEqual(lock.read_bytes(), lock_data)
        self.assertEqual(report.read_bytes(), published)
        manifest.write_text(manifest.read_text().split('define\n')[0])
        interface.write_text('on1xfile 1\ninterface\n "wrong.on1xi"\n')
        self.write('src/wrong.on1xi', 'on1xi 1;\nfn main() -> i64;\n')
        self.build(negative='project/src/On1xfile.i:2-3: interface')


if __name__ == '__main__':
    cli = argparse.ArgumentParser()
    cli.add_argument('--on1xc', required=True)
    cli.add_argument('--cc', default='cc')
    cli.add_argument('--driver', default=str(ON1X / 'tools/on1x-project.py'))
    cli.add_argument('--mune-runner')
    cli.add_argument('--qmr-compiler')
    cli.add_argument('--mulib-root')
    cli.add_argument('--archvirt-enabled', action='store_true')
    OPTIONS, remainder = cli.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remainder], verbosity=2)
