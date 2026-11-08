"""Regression checks for snapshot C literals and dependency compatibility groups."""
import hashlib
import importlib.util
import json
from pathlib import Path
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


class Tools(unittest.TestCase):
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
                '/** @brief Sample API. */\n#if defined(MULIB_IMPL_A)\n#endif\n')
            (root / 'include/mulib-future.h').write_text('/* reserved user notes */\n')
            inventory.sync(root)
            before = {p: p.read_text() for p in (root / 'MANIFEST.yaml',
                      root / 'tests/implementation.c', root / 'tests/declarations.c')}
            self.assertIn('module_count: 2', before[root / 'MANIFEST.yaml'])
            self.assertIn('implemented_module_count: 1', before[root / 'MANIFEST.yaml'])
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
