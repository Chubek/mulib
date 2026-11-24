"""Inventory and filesystem transaction tests; no claims about FFI admission."""
from copy import deepcopy
import hashlib
from pathlib import Path
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import patch


TOOLS = Path(__file__).resolve().parents[1] / 'tools'
ROOT = TOOLS.parents[3]
sys.path.insert(0, str(TOOLS))
import ffi_inventory  # noqa: E402
import ffi_publish as publication  # noqa: E402
from check_stdffi import check_counts  # noqa: E402


class InventoryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = ffi_inventory.load(ROOT)

    def test_canonical_keys(self):
        entries = self.inventory.entries
        self.assertEqual([entry.key for entry in entries], sorted(self.inventory.by_key))
        self.assertEqual(len({entry.module for entry in entries}), len(entries))
        self.assertEqual(self.inventory.index_metadata['registered_modules'], len(entries))
        aliases = {'media-timebase': 'media_timebase', '2dgeom': 'k_2dgeom',
                   'b+tree': 'b_plus_tree', 'cowb+tree': 'cowb_plus_tree',
                   'hidden_markov_chain': 'hidden_markov_chain'}
        for key, alias in aliases.items():
            with self.subTest(key=key):
                entry = self.inventory.by_key[key]
                self.assertEqual(entry.module, 'mulib.' + alias)
                self.assertEqual(entry.filename, key + '.ffi')
                self.assertEqual(entry.header_path, 'include/mulib-' + key + '.h')

    def test_real_declaration_and_native_plans(self):
        inlines = self.inventory.plan('media-timebase')
        self.assertEqual(inlines.source, b'#include <mulib-media-timebase.h>\n')
        self.assertEqual(inlines.include_dirs, (ROOT / 'include',))
        self.assertEqual(inlines.metadata['implementation'], 'static-inline')
        self.assertNotIn(b'MULIB_IMPL_', inlines.source)
        self.assertFalse(any(d.startswith('MULIB_IMPL_') for d in inlines.defines))
        self.assertIn('include/mulib-media-core.h',
                      inlines.metadata['declared_dependency_headers'])
        data = self.inventory.plan('lprowse').metadata
        self.assertEqual(data['implementation'], 'static-data')
        self.assertIn('tools/embed_lprowse.py', data['native_plan']['generator_checks'])
        cdd = self.inventory.plan('cdd-registry').metadata
        self.assertIn('include/mulib-cdd-catalogue.h', cdd['declared_dependency_headers'])
        self.assertIn('cdd-registry', cdd['native_plan']['archive_discovery_only'])
        numerical = self.inventory.plan('histprice').metadata
        self.assertIn('m', numerical['native_plan']['link_libraries'])
        parsec = self.inventory.plan('parsec').metadata
        self.assertEqual(parsec['native_plan']['recipe'], 'exoml-core')
        self.assertNotIn('MULIB_IMPL_PARSER', parsec['native_plan']['implementation_macros'])
        process = self.inventory.plan('process-stream')
        self.assertIn('_GNU_SOURCE=1', process.defines)

    def test_special_companions_do_not_enter_declaration_tu(self):
        plan = self.inventory.plan('shiraz')
        self.assertEqual(plan.source, b'#include <mulib-shiraz.h>\n')
        self.assertIn('library/shiraz/runtime.c', plan.metadata['native_plan']['native_sources'])
        plan = self.inventory.plan('yantlr')
        self.assertEqual(plan.source, b'#include <mulib-yantlr.h>\n')
        self.assertEqual(plan.metadata['native_plan']['include_headers'], [])
        provider, = plan.metadata['fixture_associations']
        self.assertFalse(provider['selected_by_default'])
        self.assertEqual(provider['kind'], 'provider')
        self.assertIn('library/yantlr/mulib-yantlr-stdlib.h',
                      provider['native_plan']['include_headers'])
        self.assertIn('MULIB_IMPL_YANTLR_STDLIB',
                      provider['native_plan']['implementation_macros'])

    def test_transitive_hash_and_input_drift(self):
        self.inventory.verify_unchanged()
        archive, = self.inventory.index_metadata['relative_include_roots']
        archived = ROOT / '.deprecated/mulib-ociimage.h'
        self.assertEqual(archive['path'], '.deprecated')
        self.assertEqual(archive['files'][archived.name], hashlib.sha256(archived.read_bytes()).hexdigest())
        self.assertEqual(self.inventory.plan('ociimage').include_dirs, (ROOT / 'include',))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            nested = root / 'detail'
            nested.mkdir()
            (root / 'entry.h').write_text('#include "detail/entry.h"\n')
            detail = nested / 'entry.h'
            detail.write_text('int a;\n')
            snapshot = ffi_inventory._include_snapshot(root)
            detail.write_text('int b;\n')
            changed = ffi_inventory._include_snapshot(root)
            self.assertEqual(snapshot['entry.h'], changed['entry.h'])
            self.assertNotEqual(snapshot['detail/entry.h'], changed['detail/entry.h'])
            (root / 'extra.h').symlink_to(detail)
            with self.assertRaisesRegex(ValueError, 'symlink'):
                ffi_inventory._include_snapshot(root)
        changed = {**self.inventory._headers, 'detail/new.h': '0' * 64}
        with patch.object(ffi_inventory, '_include_snapshot', return_value=changed):
            with self.assertRaisesRegex(ValueError, 'include tree changed'):
                self.inventory.verify_unchanged()


class PublicationTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.output = self.root / 'catalogue'
        self.target = publication.DEFAULT_TARGET

    def finish(self, payload, *, full=True, check=False, verify=lambda: None,
               validate=lambda path: None):
        with publication.transaction(self.output, self.target, check=check, full=full) as tx:
            index = {'schema': publication.SCHEMA, 'target': self.target, 'modules': []}
            for key, data in payload.items():
                name = key + '.ffi'
                if data is not None:
                    (tx.stage / name).write_bytes(data)
                else:
                    data = (tx.stage / name).read_bytes()
                index['modules'].append({'key': key, 'file': name,
                                         'ffi_sha256': hashlib.sha256(data).hexdigest()})
            tx.finish(index, validate=validate, verify=verify, expected_keys=payload)

    def test_incremental_and_check_without_writes(self):
        self.finish({'b+tree': b'old', '2dgeom': b'unchanged'})
        old_inode = self.output.stat().st_ino
        self.finish({'b+tree': b'new', '2dgeom': None}, full=False)
        self.assertNotEqual(self.output.stat().st_ino, old_inode)
        self.assertEqual((self.output / 'b+tree.ffi').read_bytes(), b'new')
        index = publication.check_package(self.output, self.target, ('b+tree', '2dgeom'))
        self.assertEqual([row['key'] for row in index['modules']], ['2dgeom', 'b+tree'])
        before = {path.name: path.stat().st_mtime_ns for path in self.output.iterdir()}
        self.finish({'b+tree': b'new', '2dgeom': b'unchanged'}, check=True)
        self.assertEqual(before, {path.name: path.stat().st_mtime_ns for path in self.output.iterdir()})

    def test_stale_removed_and_changed_files(self):
        self.finish({'clamp': b'old', 'removed': b'old'})
        before = (self.output / 'index.json').read_bytes()
        with self.assertRaisesRegex(ValueError, 'unexpected .*removed.ffi.*changed .*clamp.ffi'):
            self.finish({'clamp': b'new'}, check=True)
        self.assertEqual((self.output / 'index.json').read_bytes(), before)
        self.assertTrue((self.output / 'removed.ffi').is_file())
        self.finish({'clamp': b'new'})
        self.assertFalse((self.output / 'removed.ffi').exists())
        (self.output / 'clamp.ffi').unlink()
        with self.assertRaisesRegex(ValueError, 'missing .*clamp.ffi'):
            self.finish({'clamp': b'new'}, check=True)
        self.assertFalse((self.output / 'clamp.ffi').exists())

    def test_preserve_on_validation_and_input_failure(self):
        self.finish({'clamp': b'previous'})
        before = (self.output / 'index.json').read_bytes()
        for mode in ['validation', 'verify', 'modified']:
            with self.subTest(mode=mode):
                def validate(stage):
                    self.assertEqual((self.output / 'clamp.ffi').read_bytes(), b'previous')
                    if mode == 'validation':
                        raise ValueError('validation failure')
                    if mode == 'modified':
                        (stage / 'clamp.ffi').write_bytes(b'not the validated file')

                def verify():
                    if mode == 'verify':
                        raise ValueError('input drift')

                with self.assertRaisesRegex(ValueError, 'failure|drift|modified'):
                    self.finish({'clamp': b'replacement'}, verify=verify, validate=validate)
                self.assertEqual((self.output / 'index.json').read_bytes(), before)
        self.assertEqual(set(self.root.iterdir()), {self.output})

    def test_refuse_existing_target_drift_and_default_overwrite(self):
        self.finish({'clamp': b'old'})
        self.target = 'aarch64-linux-gnu'
        with self.assertRaisesRegex(ValueError, 'index target mismatch'):
            self.finish({'clamp': b'wrong'})
        self.output = self.root / 'on1x/stdffi/mulib'
        with self.assertRaisesRegex(ValueError, 'non-default target'):
            self.finish({'clamp': b'wrong'})
        self.assertFalse(self.output.parent.exists())
        self.output = self.root / 'aarch64'
        self.finish({'clamp': b'cross'})

    def test_refuse_index_drift_or_unindexed_directory(self):
        self.finish({'clamp': b'old'})
        extra = self.output / 'extra.ffi'
        extra.write_bytes(b'not in index')
        with self.assertRaisesRegex(ValueError, 'unexpected .*extra.ffi'):
            self.finish({'clamp': b'new'}, full=False)
        with self.assertRaisesRegex(ValueError, 'unexpected .*extra.ffi'):
            publication.check_package(self.output, self.target, ('clamp',))
        (self.output / 'index.json').unlink()
        with self.assertRaisesRegex(ValueError, 'no target index'):
            self.finish({'clamp': b'new'})

    def test_target_snapshot_and_budget_fail_closed(self):
        with self.assertRaisesRegex(ValueError, 'byte limit'):
            self.finish({'clamp': b'x' * (publication.MAX_FILE_BYTES + 1)})
        self.assertFalse(self.output.exists())
        self.finish({'clamp': b'old'})
        other = self.output / 'extra.ffi'
        other.symlink_to(self.output / 'clamp.ffi')
        with self.assertRaisesRegex(ValueError, 'regular'):
            self.finish({'clamp': b'new'})
        self.assertTrue(other.is_symlink())
        self.assertEqual((self.output / 'clamp.ffi').read_bytes(), b'old')

    def test_generation_parallel_and_ordered(self):
        rendezvous = threading.Barrier(2, timeout=5)

        def generate(number):
            rendezvous.wait()
            return number * 2

        self.assertEqual(publication.bounded_map(generate, [2, 1, 4, 3], 2), [4, 2, 8, 6])
        with self.assertRaisesRegex(ValueError, 'jobs'):
            publication.bounded_map(generate, [], publication.MAX_JOBS + 1)

    def test_full_publication_requires_expected_keys(self):
        with self.assertRaisesRegex(ValueError, 'exactly the requested manifest keys'):
            with publication.transaction(self.output, self.target, full=True) as tx:
                tx.finish({'schema': publication.SCHEMA, 'target': self.target, 'modules': []},
                          validate=lambda path: None, verify=lambda: None,
                          expected_keys=['clamp'])
        self.assertFalse(self.output.exists())

    def test_only_one_writer(self):
        with publication.transaction(self.output, self.target, full=True) as writer:
            with self.assertRaisesRegex(ValueError, 'another stdffi transaction'):
                with publication.transaction(self.output, self.target, full=True):
                    self.fail('an incompatible writer acquired the lock')
            writer.finish({'schema': publication.SCHEMA, 'target': self.target, 'modules': []},
                          validate=lambda path: None, verify=lambda: None)


class CountsTest(unittest.TestCase):
    def setUp(self):
        entries = ('clamp', 'unavailable')
        self.inventory = SimpleNamespace(
            entries=entries, index_metadata={'manifest': {}},
            by_key={key: SimpleNamespace(module='mulib.' + key,
                                        header_path=f'include/mulib-{key}.h') for key in entries})
        self.index = {
            'inventory': self.inventory.index_metadata, 'unattempted': [],
            'modules': [{
                'key': 'clamp', 'module': 'mulib.clamp', 'header': 'include/mulib-clamp.h',
                'status': 'representable', 'attempted': True, 'projected': True,
                'projection_sha256': '0' * 64, 'normalized_signature_sha256': '1' * 64,
                'function_count': 3, 'symbols': [
                    {'symbol': 'mulib_clamp_i64', 'status': 'representable',
                     'ownership': 'audited_selection', 'emitted': True},
                    {'symbol': 'mulib_clamp_unknown', 'status': 'discovery_only',
                     'ownership': 'ambiguous_export', 'reason': 'ambiguous_export', 'emitted': True},
                ],
            }, {
                'key': 'unavailable', 'module': 'mulib.unavailable',
                'header': 'include/mulib-unavailable.h', 'status': 'failed',
                'attempted': True, 'projected': False, 'reason': 'interface_limit',
                'projection_sha256': '2' * 64, 'symbols': [],
            }],
            'counts': {
                'registered_modules': 2, 'attempted_modules': 2, 'projected_modules': 1,
                'unattempted_modules': 0, 'module_outcomes': {'representable': 1, 'failed': 1},
                'symbol_outcomes': {'representable': 1, 'discovery_only': 1},
                'projected_functions': 3, 'non_candidate_functions': 1,
                'owned_functions': 1, 'emitted_scalar_candidates': 1,
            },
        }

    def test_index_reconciles_distinct_populations(self):
        check_counts(self.index, self.inventory)
        for field, value in [('projected_modules', 2), ('owned_functions', 2),
                             ('emitted_scalar_candidates', True)]:
            index = deepcopy(self.index)
            index['counts'][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'do not reconcile'):
                check_counts(index, self.inventory)

    def test_index_refuses_inconsistent_outcomes(self):
        for row, field, value, message in (
                (0, 'header', 'include/mulib-wrong.h', 'module identity'),
                (0, 'projection_sha256', None, 'projection digest'),
                (1, 'normalized_signature_sha256', '0' * 64, 'projection digest'),
                (1, 'reason', None, 'module reason')):
            index = deepcopy(self.index)
            index['modules'][row][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
                check_counts(index, self.inventory)
        first = self.index['modules'][0]['symbols'][0]
        first['ownership'] = 'ambiguous_export'
        with self.assertRaisesRegex(ValueError, 'inconsistent stdffi symbol state'):
            check_counts(self.index, self.inventory)


if __name__ == '__main__':
    unittest.main()
