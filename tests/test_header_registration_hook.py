"""Integration tests for the header-registration hook in disposable Git repos."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
CATALOG = 'on1x/contracts-exotract/kernel-auto.json'
HEADER = 'include/mulib-added.h'
MANUAL = 'manual/mulib-added.md'
OLD_HEADER = 'include/mulib-base.h'
OLD_MANUAL = 'manual/mulib-base.md'
NEW_API = ('/** @brief The added public API. */\n'
           '#include "mulib-base.h"\n'
           'int added(void);\n'
           '#if defined(MULIB_IMPL_ADDED)\n'
           'int added(void) { return 1; }\n'
           '#endif\n')
IMPLEMENTATION = 'tests/implementation.c'
DECLARATIONS = 'tests/declarations.c'


@unittest.skipUnless(shutil.which('git') and shutil.which('python3'), 'Git and python3 required')
class HeaderRegistrationHook(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / 'build'
        temp = tempfile.TemporaryDirectory(dir=scratch if scratch.is_dir() else None)
        self.addCleanup(temp.cleanup)
        self.directory = Path(temp.name)
        self.repo = self.directory / 'working tree'
        self.repo.mkdir()
        self.env = {key: value for key, value in os.environ.items() if not key.startswith('GIT_')}
        self.env.update(GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull)
        self.git('init', '--quiet', '--template=')
        self.git('config', 'user.name', 'Hook test')
        self.git('config', 'user.email', 'hook-test@example.test')
        self.git('config', 'core.autocrlf', 'false')
        for name in ('.githooks/pre-commit', 'tools/install_hooks.py',
                     'tools/header_registration_hook.py', 'tools/sync_inventory.py',
                     'tools/extend-kernelbind.py', 'on1x/tools/on1x-contracts.py'):
            destination = self.repo / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, destination)
        (self.repo / '.githooks/pre-commit').chmod(0o755)
        self.put(OLD_HEADER, '#if defined(MULIB_IMPL_BASE)\n#endif\n')
        self.put(OLD_MANUAL, '# Base API\n\nBase module contract.\n')
        self.put('MANIFEST.yaml',
                 'module_count: 1\nimplemented_module_count: 1\nbounded_module_count: 0\n'
                 'modules:\n- name: base\n  header: include/mulib-base.h\n'
                 '  macro: MULIB_IMPL_BASE\n  status: implemented\n'
                 '  purpose: Base module.\n  manual: manual/mulib-base.md\n  dependencies: []\n'
                 'project_files:\n')
        self.put(CATALOG, '{}\n')
        self.put('notes.txt', 'committed notes\n')
        self.put(IMPLEMENTATION, '/* baseline implementation */\n')
        self.put(DECLARATIONS, 'int main(void) { return 0; }\n')
        self.run_process(sys.executable, 'tools/sync_inventory.py', '--metadata-only')
        self.git('add', '--', '.')
        self.git('commit', '--quiet', '-m', 'baseline')

    def put(self, name, content):
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content.encode('utf-8'))
        return path

    def read(self, name):
        return (self.repo / name).read_bytes().decode('utf-8')

    def run_process(self, *command, ok=True, env=None, cwd=None):
        result = subprocess.run(command, cwd=self.repo if cwd is None else cwd,
                                env=self.env if env is None else env, capture_output=True, text=True)
        if ok:
            self.assertEqual(result.returncode, 0, msg=result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, msg=result.stdout + result.stderr)
        return result

    def git(self, *args, **kwargs):
        return self.run_process('git', *args, **kwargs)

    def install(self, **kwargs):
        return self.run_process(sys.executable, 'tools/install_hooks.py', **kwargs)

    def add_api(self, *, stage_manual=True):
        self.put(HEADER, NEW_API)
        self.put(MANUAL, '# Added public API\n\nContract written by the maintainer.\n')
        self.git('add', '--', HEADER, *((MANUAL,) if stage_manual else ()))

    def changed(self):
        return set(self.git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD')
                   .stdout.splitlines())

    def verify_registration(self, extra=()):
        self.assertEqual(self.changed(), {HEADER, MANUAL, 'MANIFEST.yaml', CATALOG, *extra})
        manifest = self.git('show', 'HEAD:MANIFEST.yaml').stdout
        self.assertIn('- name: added\n  header: include/mulib-added.h\n', manifest)
        self.assertIn('  dependencies: [base]\n', manifest)
        catalog = json.loads(self.git('show', f'HEAD:{CATALOG}').stdout)
        self.assertEqual(catalog['modules']['added']['macro'], 'MULIB_IMPL_ADDED')
        self.assertEqual(catalog['modules']['added']['dependencies'], ['base'])
        self.assertEqual(self.git('diff', '--cached', '--name-only').stdout, '')

    def test_real_commit_uses_staged_bytes_and_leaves_local_edits_unstaged(self):
        self.install()
        self.add_api()
        local = {
            HEADER: '#include "mulib-untracked-dependency.h"\n' + NEW_API,
            MANUAL: 'unstaged copy of the manual\n',
            'notes.txt': 'unrelated unstaged notes\n',
            IMPLEMENTATION: 'local implementation fixture\n',
            DECLARATIONS: 'local declarations fixture\n',
            'include/mulib-untracked.h': 'untracked header with no implementation metadata\n',
            'MANIFEST.yaml.additions': 'untracked fragment, not generator input\n',
        }
        for path, content in local.items():
            self.put(path, content)
        self.put('staged elsewhere.txt', 'user already staged this file\n')
        self.git('add', '--', 'staged elsewhere.txt')
        self.git('commit', '--quiet', '-m', 'add header')
        self.verify_registration(extra=('staged elsewhere.txt',))
        self.assertEqual(self.git('show', f'HEAD:{HEADER}').stdout, NEW_API)
        self.assertIn('Contract written by the maintainer.', self.git('show', f'HEAD:{MANUAL}').stdout)
        self.assertEqual({path: self.read(path) for path in local}, local)
        self.assertEqual(self.git('diff', '--name-only', '--', 'MANIFEST.yaml', CATALOG).stdout, '')

    def test_unrelated_commit_skips_generator_even_with_unstaged_new_header(self):
        self.install()
        self.put('tools/sync_inventory.py', 'raise RuntimeError("generator invoked")\n')
        self.put(HEADER, NEW_API)
        self.git('add', '-N', '--', HEADER)  # Intent-to-add is not a staged blob.
        self.put(OLD_HEADER, self.read(OLD_HEADER) + '/* existing header edited */\n')
        self.put('notes.txt', 'update unrelated notes\n')
        self.git('add', '--', OLD_HEADER, 'notes.txt')
        self.git('commit', '--quiet', '-m', 'no public header addition')
        self.assertEqual(self.changed(), {'notes.txt', OLD_HEADER})

    def test_refuses_unstaged_edit_to_either_changed_output(self):
        self.install()
        self.add_api()
        for path in ('MANIFEST.yaml', CATALOG):
            with self.subTest(path=path):
                other = CATALOG if path == 'MANIFEST.yaml' else 'MANIFEST.yaml'
                original = self.read(path)
                staged = original + '\n'
                self.put(path, staged)
                self.git('add', '--', path)
                local = staged + '# local edits\n'
                self.put(path, local)
                other_before = self.read(other)
                index = self.git('write-tree').stdout
                result = self.git('commit', '--quiet', '-m', 'partial output', ok=False)
                self.assertIn(f'{path} differs from the staged snapshot', result.stderr)
                self.assertEqual(self.read(path), local)
                self.assertEqual(self.read(other), other_before)
                self.assertEqual(self.git('write-tree').stdout, index)
                self.put(path, original)
                self.git('add', '--', path)

    def test_unstaged_manual_is_not_authoritative(self):
        self.install()
        self.add_api(stage_manual=False)
        index = self.git('write-tree').stdout
        result = self.git('commit', '--quiet', '-m', 'missing staged manual', ok=False)
        self.assertIn(MANUAL, result.stderr)
        self.assertEqual(self.git('write-tree').stdout, index)
        self.git('add', '--', MANUAL)
        self.git('commit', '--quiet', '-m', 'stage manual')
        self.verify_registration()

    def test_missing_implementation_requires_explicit_metadata(self):
        self.install()
        self.add_api()
        self.put(HEADER, 'static inline int added(void) { return 1; }\n')
        self.git('add', '--', HEADER)
        index = self.git('write-tree').stdout
        result = self.git('commit', '--quiet', '-m', 'missing implementation metadata', ok=False)
        self.assertIn('implementation', result.stderr)
        self.assertIn('mulib-added.h', result.stderr)
        self.assertEqual(self.git('write-tree').stdout, index)

    def test_bounded_entry_and_comment_only_reservation(self):
        self.install()
        for name, implementation, content in (
            ('added-inline', 'static-inline', 'static inline int added(void) { return 1; }\n'),
            ('added-data', 'static-data', 'static const int added = 1;\n'),
        ):
            self.put(f'include/mulib-{name}.h', content)
            self.put(f'manual/mulib-{name}.md', '# Bounded header\n')
            entry = (f'- name: {name}\n  header: include/mulib-{name}.h\n'
                     f'  manual: manual/mulib-{name}.md\n  status: bounded\n'
                     f'  implementation: {implementation}\n  dependencies: []\n')
            self.put('MANIFEST.yaml', self.read('MANIFEST.yaml').replace(
                'project_files:', entry + 'project_files:'))
            self.git('add', '--', f'include/mulib-{name}.h', f'manual/mulib-{name}.md')
        placeholder = 'include/mulib-added-reservation.h'
        self.put(placeholder, '/* No API yet. */\n')
        self.git('add', '--', 'MANIFEST.yaml', placeholder)
        self.git('commit', '--quiet', '-m', 'add internal linkage headers and reservation')
        catalog = json.loads(self.git('show', f'HEAD:{CATALOG}').stdout)['modules']
        self.assertEqual(catalog['added-inline']['implementation'], 'static-inline')
        self.assertEqual(catalog['added-data']['implementation'], 'static-data')
        self.assertNotIn('added-reservation', catalog)
        self.assertIn(f'header: {placeholder}', self.git('show', 'HEAD:MANIFEST.yaml').stdout)
        self.assertEqual(self.git('diff', '--cached', '--name-only').stdout, '')

    def test_refuses_staged_additions_fragment_without_consuming_it(self):
        self.install()
        self.add_api()
        for path in ('MANIFEST.yaml.additions', 'MANIFESTS.yaml.additions'):
            with self.subTest(path=path):
                self.put(path, 'fragment contents\n')
                self.git('add', '--', path)
                index = self.git('write-tree').stdout
                result = self.git('commit', '--quiet', '-m', 'fragment', ok=False)
                self.assertIn(path, result.stderr)
                self.assertIn('python3 tools/sync_inventory.py manually', result.stderr)
                self.assertEqual(self.read(path), 'fragment contents\n')
                self.assertEqual(self.git('write-tree').stdout, index)
                self.git('rm', '--cached', '--', path)

    def test_path_limited_commit_requires_normal_staging(self):
        self.install()
        self.add_api()
        index = self.git('write-tree').stdout
        manifest = self.read('MANIFEST.yaml')
        catalog = self.read(CATALOG)
        result = self.git('commit', '--quiet', '-m', 'only these paths', '--only', '--',
                          HEADER, MANUAL, ok=False)
        self.assertIn('path-limited commit', result.stderr)
        self.assertEqual(self.read('MANIFEST.yaml'), manifest)
        self.assertEqual(self.read(CATALOG), catalog)
        self.assertEqual(self.git('write-tree').stdout, index)
        self.git('commit', '--quiet', '-m', 'normal commit')
        self.verify_registration()

    def test_alternate_index_is_preserved(self):
        self.install()
        env = {**self.env, 'GIT_INDEX_FILE': str(self.directory / 'alternate index')}
        self.git('read-tree', 'HEAD', env=env)
        self.put(HEADER, NEW_API)
        self.put(MANUAL, '# Added API\n\nModule contract.\n')
        self.git('add', '--', HEADER, MANUAL, env=env)
        index = self.git('write-tree').stdout
        alternative = self.git('write-tree', env=env).stdout
        manifest = self.read('MANIFEST.yaml')
        catalog = self.read(CATALOG)
        result = self.git('commit', '--quiet', '-m', 'alternate', env=env, ok=False)
        self.assertIn('alternate Git index', result.stderr)
        self.assertEqual(self.read('MANIFEST.yaml'), manifest)
        self.assertEqual(self.read(CATALOG), catalog)
        self.assertEqual(self.git('write-tree').stdout, index)
        self.assertEqual(self.git('write-tree', env=env).stdout, alternative)

    def test_commit_all_updates_the_git_lock_index(self):
        self.install()
        self.add_api()
        self.git('commit', '--quiet', '-a', '-m', 'commit all')
        self.verify_registration()

    def test_commit_include_updates_the_git_lock_index(self):
        self.install()
        self.add_api()
        self.put('notes.txt', 'changes to include in the commit\n')
        self.git('commit', '--quiet', '-m', 'include path', '--include', '--', 'notes.txt')
        self.verify_registration(extra=('notes.txt',))

    def test_linked_worktree_uses_the_shared_dispatcher_and_its_own_index(self):
        main = self.repo
        linked = self.directory / 'linked tree'
        self.git('worktree', 'add', '--quiet', '--detach', str(linked))
        self.repo = linked
        self.install()
        self.add_api()
        self.git('commit', '--quiet', '-m', 'linked checkout')
        self.verify_registration()
        self.assertTrue((main / '.git/hooks/pre-commit').is_file())
        self.assertEqual(self.git('status', '--short', cwd=main).stdout, '')

    def test_installer_preserves_existing_hook_and_custom_path(self):
        target = self.repo / '.git/hooks/pre-commit'
        content = '#!/bin/sh\nexit 37\n'
        self.put('.git/hooks/pre-commit', content).chmod(0o755)
        result = self.install(ok=False)
        self.assertIn('already exists', result.stderr)
        self.assertEqual(target.read_bytes().decode(), content)
        self.assertEqual(target.stat().st_mode & 0o777, 0o755)
        self.git('config', 'core.hooksPath', 'my hooks')
        custom = self.put('my hooks/pre-commit', content)
        result = self.install(ok=False)
        self.assertIn('core.hooksPath is already configured', result.stderr)
        self.assertEqual(custom.read_bytes().decode(), content)
        self.assertEqual(self.git('config', '--get', 'core.hooksPath').stdout, 'my hooks\n')

    def test_installer_is_idempotent_and_does_not_set_git_config(self):
        self.install()
        self.assertIn('already installed', self.install().stdout)
        self.assertEqual(self.git('config', '--get', 'core.hooksPath', ok=False).returncode, 1)


if __name__ == '__main__':
    unittest.main()
