"""Regression checks for snapshot C literals and dependency compatibility groups."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parent.parent


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


snapshot = module('check_snapshot')
strata = module('stratify')
inventory = module('sync_inventory')
manuals = module('manuals')
graphics = module('graphics_inventory')


class Tools(unittest.TestCase):
    def test_graphics_inventory_preserves_bounded_status(self):
        parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
        with tempfile.TemporaryDirectory(dir=parent) as tmp:
            root = Path(tmp)
            (root / 'include').mkdir()
            (root / 'tests').mkdir()
            (root / 'tests/implementation.c').write_text('/* ABI TU */\n')
            (root / 'tests/declarations.c').write_text('int main(void) { return 0; }\n')
            (root / 'include/mulib-a.h').write_text(
                '#if defined(MULIB_IMPL_A)\n#endif\n')
            (root / 'include/mulib-glx-core.h').write_text(
                'static inline int mulib_gfx_core(void) { return 7; }\n')
            (root / 'include/mulib-glx-surface.h').write_text(
                '#include "mulib-glx-core.h"\n')
            (root / 'include/mulib-glx-scene.h').write_text(
                'static inline int mulib_scene(void) { return 1; }\n')
            notebook = ('- name: glx-scene\n  header: include/mulib-glx-scene.h\n'
                        '  implementation: static-inline\n  status: bounded\n'
                        '  profile: notebook-desktop\n'
                        '  purpose: Independent notebook scene.\n  dependencies: []\n')
            manifest = root / 'MANIFEST.yaml'
            manifest.write_text(
                'module_count: 1\nimplemented_module_count: 1\nmodules:\n'
                '- name: a\n  header: include/mulib-a.h\n'
                '  purpose: User-authored scope.\n  status: implemented\n'
                '  macro: MULIB_IMPL_A\n  dependencies: []\n' + notebook +
                'project_files:\n- file: user.c\n  purpose: User work.\n')
            manifest.write_text(graphics.updated_manifest(root))
            expected = manifest.read_text()
            self.assertIn('module_count: 4\n', expected)
            self.assertIn('bounded_module_count: 3\n', expected)
            self.assertEqual(expected.count('  status: bounded\n'), 3)
            self.assertIn(notebook, expected)
            self.assertEqual([p.name for p in graphics.headers(root)],
                             ['mulib-glx-core.h', 'mulib-glx-surface.h'])
            self.assertIn('  dependencies: [glx-core]\n', expected)
            self.assertIn('purpose: User-authored scope.', expected)
            self.assertIn('- file: user.c\n  purpose: User work.', expected)
            self.assertEqual(expected, graphics.updated_manifest(root))
            inventory.sync(root)
            self.assertEqual((root / 'tests/implementation.c').read_text().count(
                '#include "mulib-glx-'), 0)
            self.assertIn('  status: bounded\n', manifest.read_text())
            self.assertIn('implemented_module_count: 1\n', manifest.read_text())
            (root / 'include/mulib-glx-core.h').write_text(
                '#if defined(MULIB_IMPL_GLX_CORE)\n#endif\n')
            with self.assertRaisesRegex(ValueError, 'ABI switch'):
                graphics.updated_manifest(root)

    def test_graphics_inventory_rejects_missing_dependencies(self):
        parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
        with tempfile.TemporaryDirectory(dir=parent) as tmp:
            root = Path(tmp)
            (root / 'include').mkdir()
            (root / 'include/mulib-glx-core.h').write_text(
                '#include "mulib-missing.h"\n')
            (root / 'MANIFEST.yaml').write_text(
                'modules:\n- name: a\n  header: include/mulib-a.h\n'
                'project_files:\n')
            with self.assertRaisesRegex(ValueError, 'unknown dependency'):
                graphics.updated_manifest(root)

    def test_checker_compile_budget(self):
        for value in ('0', '-1', 'invalid'):
            with self.subTest(value=value):
                result = subprocess.run(
                    [sys.executable, str(ROOT / 'tools/check.py'),
                     '--compile-timeout', value], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('--compile-timeout', result.stderr)
        result = subprocess.run(
            [sys.executable, str(ROOT / 'tools/check.py'),
             '--compile-timeout', '300', '--help'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)

    def test_manual_sections_preserve_prose(self):
        before = ('# User title\n\nContract written by a maintainer.\n\n'
                  '<!-- manual-interface -->\nold\n<!-- /manual-interface -->\n'
                  '\nAdditional handwritten notes.\n')
        after = manuals.replace_section(before, 'interface', 'new reference')
        self.assertEqual(after, before.replace('\nold\n', '\nnew reference\n'))
        self.assertEqual(after, manuals.replace_section(after, 'interface', 'new reference'))
        for broken in (before.replace('<!-- /manual-interface -->', ''),
                       before + before,
                       '<!-- /manual-interface -->\n<!-- manual-interface -->'):
            with self.subTest(broken=broken), self.assertRaises(ValueError):
                manuals.replace_section(broken, 'interface', 'new')

    def test_manual_links(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            page = directory / 'manual.md'
            target = directory / 'with space.md'
            documents = {
                page: ('# Reference\n[valid](with%20space.md#api-1)\n'
                       '[`source`](with%20space.md)\n'
                       '`[example](NOT-A-LINK)`\n'
                       '```c\n[example](NOT-A-LINK)\n```\n'
                       '[external](https://example.com/missing)\n'),
                target: '# API\n## API\n',
            }
            self.assertEqual(manuals.check_links(documents), [])
            documents[page] += '[broken](with%20space.md#missing)\n[absent](absent.md)\n'
            failures = manuals.check_links(documents)
            self.assertEqual(len(failures), 2)
            self.assertIn('missing anchor', failures[0])
            self.assertIn('missing link', failures[1])

    def test_inventory_sync(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'include').mkdir()
            (root / 'tests').mkdir()
            (root / 'MANIFEST.yaml').write_text(
                'module_count: 0\nimplemented_module_count: 0\nmodules:\nproject_files:\n')
            (root / 'tests/implementation.c').write_text('/* existing client */\n')
            (root / 'tests/declarations.c').write_text('int main(void) { return 0; }\n')
            (root / 'include/mulib-a.h').write_text(
                '/** @brief Sample API. */\n#include "mulib-container-core.h"\n#if defined(MULIB_IMPL_A)\n#endif\n')
            (root / 'include/mulib-container-core.h').write_text(
                'static inline int mulib_ct_support(void) { return 1; }\n')
            (root / 'include/mulib-future.h').write_text('/* reserved user notes */\n')
            inventory.sync(root)
            before = {p: p.read_text() for p in (root / 'MANIFEST.yaml',
                      root / 'tests/implementation.c', root / 'tests/declarations.c')}
            self.assertIn('module_count: 2', before[root / 'MANIFEST.yaml'])
            self.assertIn('implemented_module_count: 1', before[root / 'MANIFEST.yaml'])
            self.assertNotIn('- name: container-core', before[root / 'MANIFEST.yaml'])
            self.assertIn('dependencies: []', before[root / 'MANIFEST.yaml'])
            self.assertIn('status: reserved', before[root / 'MANIFEST.yaml'])
            self.assertIn('#define MULIB_IMPL_A', before[root / 'tests/implementation.c'])
            inventory.sync(root)
            self.assertEqual(before, {p: p.read_text() for p in before})
            (root / 'include/mulib-a.h').write_text('/* API accidentally erased */\n')
            with self.assertRaisesRegex(ValueError, 'lost its API'):
                inventory.sync(root)
            self.assertEqual(before, {p: p.read_text() for p in before})
            (root / 'include/mulib-a.h').write_text(
                '/** @brief Sample API. */\n#if defined(MULIB_IMPL_A)\n#endif\n')
            # Promote a reserved module and refresh dependencies, preserving
            # maintainer prose and custom implementation switches.
            path = root / 'MANIFEST.yaml'
            path.write_text(path.read_text().replace(
                '  purpose: "Sample API."',
                '  purpose: "Maintainer description"\n  extra_macros: [MULIB_IMPL_EXTRA]'))
            (root / 'include/mulib-future.h').write_text(
                '#include "mulib-a.h"\n#if defined(MULIB_IMPL_FUTURE)\n#endif\n')
            inventory.sync(root)
            self.assertNotIn('status: reserved', path.read_text())
            self.assertIn('dependencies: [a]', path.read_text())
            self.assertIn('Maintainer description', path.read_text())
            self.assertIn('extra_macros: [MULIB_IMPL_EXTRA]', path.read_text())
            self.assertIn('#define MULIB_IMPL_FUTURE',
                          (root / 'tests/implementation.c').read_text())
            # An invalid client file must not leave the manifest half-updated.
            (root / 'include/mulib-b.h').write_text('/* future */\n')
            (root / 'tests/declarations.c').write_text('/* no main */\n')
            before = {p: p.read_text() for p in before}
            with self.assertRaisesRegex(ValueError, 'insertion point'):
                inventory.sync(root)
            self.assertEqual(before, {p: p.read_text() for p in before})

    def test_amb3r_inventory_retains_shared_manual(self):
        original_manuals, original_strata = manuals.ROOT, strata.ROOT
        try:
            with tempfile.TemporaryDirectory() as tmp:
                root = manuals.ROOT = strata.ROOT = Path(tmp)
                for name in ('include', 'tests', 'manual'):
                    (root / name).mkdir()
                implementation = root / 'tests/implementation.c'
                declarations = root / 'tests/declarations.c'
                implementation.write_text('/* legacy ABI TU */\n')
                declarations.write_text('int main(void) { return 0; }\n')
                for stem in manuals.AUXILIARY:
                    (root / 'include' / (stem + '.h')).touch()
                    (root / 'manual' / (stem + '.md')).touch()
                (root / 'examples/modules').mkdir(parents=True)
                for name in manuals.AUXILIARY.values():
                    (root / 'examples/modules' / (name + '.c')).touch()
                (root / 'manual/amb3r-kernels.md').write_text('# Reference kernels\n')
                (root / 'include/mulib-amb3r-base.h').write_text(
                    'static inline int ma_identity(int n) { return n; }\n')
                (root / 'include/mulib-amb3r-example.h').write_text(
                    '#include "mulib-amb3r-base.h"\n#if defined(MULIB_IMPL_AMB3R_EXAMPLE)\n#endif\n')
                manifest = root / 'MANIFEST.yaml'
                manifest.write_text(
                    'module_count: 0\nimplemented_module_count: 0\nbounded_module_count: 0\n'
                    'modules:\n'
                    '- name: amb3r-base\n  header: include/mulib-amb3r-base.h\n'
                    '  manual: manual/amb3r-kernels.md\n  profile: amb3r\n'
                    '  implementation: static-inline\n  status: bounded\n  dependencies: []\n'
                    '- name: amb3r-example\n  header: include/mulib-amb3r-example.h\n'
                    '  manual: manual/amb3r-kernels.md\n  profile: amb3r\n'
                    '  macro: MULIB_IMPL_AMB3R_EXAMPLE\n  status: implemented\n'
                    '  dependencies: []\nproject_files:\n')
                inventory.sync(root)
                result = manifest.read_text()
                self.assertIn('module_count: 2\n', result)
                self.assertIn('implemented_module_count: 1\n', result)
                self.assertIn('bounded_module_count: 1\n', result)
                self.assertIn('  dependencies: [amb3r-base]\n', result)
                self.assertNotIn('amb3r', implementation.read_text())
                self.assertNotIn('amb3r', declarations.read_text())
                records = [r for r in manuals.inventory() if r['header'].name.startswith('mulib-amb3r')]
                self.assertEqual(len(records), 2)
                for record in records:
                    self.assertEqual(record['manual'].name, 'amb3r-kernels.md')
                    self.assertTrue(record['shared'])
                    self.assertIsNone(record['example'])
                self.assertIn('[amb3r-example](amb3r-kernels.md)', manuals.index(records))
                self.assertIn('[amb3r-example](amb3r-kernels.md)', strata.render())
        finally:
            manuals.ROOT, strata.ROOT = original_manuals, original_strata

    def test_inventory_additions(self):
        for filename in ('MANIFEST.yaml.additions', 'MANIFESTS.yaml.additions'):
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                for name in ('include', 'tests', 'examples'):
                    (root / name).mkdir()
                manifest = root / 'MANIFEST.yaml'
                manifest.write_text('name: MuLIB\nmodule_count: 0\n'
                                    'implemented_module_count: 0\nmodules:\nproject_files:\n')
                (root / 'tests/implementation.c').write_text('/* existing client */\n')
                (root / 'tests/declarations.c').write_text('int main(void) { return 0; }\n')
                (root / 'include/mulib-core.h').write_text(
                    '#if defined(MULIB_IMPL_CORE)\n#endif\n')
                (root / 'include/mulib-new.h').write_text(
                    '#include "mulib-core.h"\n#if defined(MULIB_IMPL_NEW)\n#endif\n')
                (root / 'examples/mulib-new.c').write_text('int main(void) { return 0; }\n')
                inventory.sync(root)
                # Keep a pre-existing module and handwritten project-file entry.
                original = manifest.read_text()
                existing = original.split('modules:\n', 1)[1].split('project_files:')[0]
                new_block = existing[existing.index('- name: new\n'):]
                manifest.write_text(original.replace(new_block, '').replace(
                    'project_files:\n', 'project_files:\n- file: USER\n  purpose: Keep me.\n'))
                additions = root / filename
                additions.write_text(
                    'name: Additions\nmodule_count: 999\nimplemented_module_count: 999\nmodules:\n'
                    '- name: new\n  header: include/mulib-new.h\n'
                    '  manual: manual/mulib-new.md\n  macro: MULIB_IMPL_NEW\n'
                    '  purpose: Checked table view only, not a complete shaper.\n'
                    '  dependencies:\n  - core\n  status: implemented\n'
                    'license: Do not copy this root key\n'
                    'project_files:\n- file: ADDITIONS-ONLY\n  purpose: Not a module.\n')
                inventory.sync(root)
                merged = manifest.read_text()
                self.assertTrue(merged.startswith('name: MuLIB\n'))
                self.assertEqual(merged.count('\nmodules:\n'), 1)
                self.assertNotIn('Do not copy', merged)
                self.assertNotIn('ADDITIONS-ONLY', merged)
                self.assertIn('module_count: 2\nimplemented_module_count: 2\n', merged)
                self.assertIn('dependencies: [core]\n', merged)
                self.assertNotIn('\n  - core\n', merged)
                self.assertIn('not a complete shaper.', merged)
                self.assertIn('- file: USER\n  purpose: Keep me.\n', merged)
                self.assertIn('  example: examples/mulib-new.c\n', merged)
                self.assertLess(merged.index('- name: core'), merged.index('- name: new'))
                self.assertLess(merged.index('- name: new'), merged.index('project_files:'))
                self.assertFalse(additions.exists())
                before = {p: p.read_text() for p in (manifest, root / 'tests/implementation.c',
                                                    root / 'tests/declarations.c')}
                inventory.sync(root)
                self.assertEqual(before, {p: p.read_text() for p in before})
                self.assertEqual(before[root / 'tests/implementation.c'].count(
                    '#define MULIB_IMPL_CORE\n'), 1)
                # Every collision must fail before changing files or deleting the additions.
                for field, value in (('name', 'core'), ('header', 'include/mulib-core.h'),
                                     ('macro', 'MULIB_IMPL_CORE')):
                    block = ('- name: other\n  header: include/mulib-other.h\n'
                             '  macro: MULIB_IMPL_OTHER\n  status: implemented\n')
                    old = {'name': '- name: other', 'header': '  header: include/mulib-other.h',
                           'macro': '  macro: MULIB_IMPL_OTHER'}[field]
                    prefix = '- ' if field == 'name' else '  '
                    additions.write_text('modules:\n' + block.replace(old, prefix + field + ': ' + value))
                    (root / 'include/mulib-other.h').write_text(
                        '#if defined(MULIB_IMPL_OTHER)\n#endif\n')
                    with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'collision'):
                        inventory.sync(root)
                    self.assertTrue(additions.exists())
                    self.assertEqual(before, {p: p.read_text() for p in before})

    def test_snapshot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'include').mkdir()
            (root / 'library/dsymspec/schema').mkdir(parents=True)
            source, schema = b'schema: 1\n', b'{}\n'
            (root / 'library/dsymspec/test.dsymspec.yaml').write_bytes(source)
            (root / 'library/dsymspec/schema/dsymspec.schema.json').write_bytes(schema)
            data = {'_mulib_codegen': {
                'source': 'dsymspec/test.dsymspec.yaml',
                'source_sha256': hashlib.sha256(source).hexdigest(),
                'schema_sha256': hashlib.sha256(schema).hexdigest()}}
            wire = json.dumps(data)
            # C concatenates adjacent literals before treating the comma as
            # the end of the array element; formatting must not break checks.
            literals = '\n'.join('  ' + json.dumps(wire[i:i+23])
                                 for i in range(0, len(wire), 23))
            text = ('static const char *const mulib_dsymspec_chunks_0_[] = {\n'
                    + literals + ',\n  NULL\n};\n')
            path = root / 'include/mulib-dsymspec-data.inc'
            path.write_text(text)
            self.assertEqual(snapshot.verify(root), 1)
            for invalid in (text.replace('  NULL\n', ''),
                            text.replace('  NULL\n', '  NULL\n  NULL\n'),
                            text.replace('  NULL\n', '  NULL\n  "tail",\n')):
                with self.subTest(invalid=invalid):
                    path.write_text(invalid)
                    with self.assertRaises(ValueError):
                        snapshot.verify(root)
            path.write_text(text)
            (root / 'library/dsymspec/test.dsymspec.yaml').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'stale architecture'):
                snapshot.verify(root)

    def test_strata(self):
        original = strata.ROOT
        try:
            with tempfile.TemporaryDirectory() as tmp:
                strata.ROOT = Path(tmp)
                directory = strata.ROOT / 'include'
                directory.mkdir()
                (directory / 'mulib-a.h').write_text('#include "mulib-b.h"\n')
                (directory / 'mulib-b.h').write_text('#include "mulib-a.h"\n')
                (directory / 'mulib-c.h').write_text('#include "mulib-a.h"\n')
                result = strata.render()
                self.assertIn('Compatibility group: `a`, `b`.', result)
                self.assertIn('Stratum 0 (2 modules)', result)
                self.assertIn('Stratum 1 (1 modules)', result)
                (directory / 'mulib-b.h').write_text('#include "mulib-missing.h"\n')
                with self.assertRaisesRegex(ValueError, 'unknown module'):
                    strata.render()
        finally:
            strata.ROOT = original


if __name__ == '__main__':
    unittest.main()
