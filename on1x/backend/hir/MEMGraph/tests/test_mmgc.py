#!/usr/bin/env python3
"""Fail-closed tests for the independent MEMGraph specification checker."""
from collections import Counter
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROFILE = 'c11_fixed_array_v1'
PASS = '50-c11-fixed-array.mmg'
RULES = 'c11-fixed-array.mmgi'
PROFILE_CLAUSE = f'profile {PROFILE};\n'
sys.path.insert(0, str(ROOT / 'tools'))
import mmgc  # noqa: E402
import preview  # noqa: E402


class MemgraphChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        generated = os.environ.get('ON1X_MEMGRAPH_TEST_PARSER')
        cls.parser = Path(generated) if generated else None

    def compile_package(self, root):
        return mmgc.compile_package(root, parser=self.parser)

    def copy(self):
        work = tempfile.TemporaryDirectory(prefix='memgraph-test-')
        self.addCleanup(work.cleanup)
        dst = Path(work.name)
        for file in ROOT.iterdir():
            if file.suffix in {'.mmg', '.mmgi', '.llsynth'}:
                shutil.copy2(file, dst / file.name)
        return dst

    def must_fail(self, root, expected=None):
        with self.assertRaises(mmgc.SpecError) as ctx:
            self.compile_package(root)
        if expected is not None:
            self.assertIn(expected, str(ctx.exception))

    def replace(self, root, path, old, new):
        p = root / path
        original = p.read_text(encoding='utf-8')
        self.assertIn(old, original)
        p.write_text(original.replace(old, new, 1), encoding='utf-8')

    def test_clean_and_deterministic(self):
        root = self.copy()
        one = self.compile_package(root)
        two = self.compile_package(root)
        self.assertEqual(one, two)
        if self.parser is not None:
            self.assertEqual(one, mmgc.compile_package(root), 'generated and bootstrap parser parity')
        self.assertEqual(one['source_type'], 'untrusted_declarative')
        self.assertEqual(one['host_admission'], 'required_not_implemented')
        self.assertEqual([x['name'] for x in one['passes']],
                         ['types', 'memory', 'ownership', 'ffi', 'canonical', PROFILE])
        self.assertEqual(len(one['inputs_sha256']), 17)

        original, profile = one['passes'][:-1], one['passes'][-1]
        self.assertTrue(all('profile' not in p for p in original))
        self.assertEqual(Counter(r['kind'] for p in original for r in p['rules']),
                         {'check': 17, 'reject': 10, 'rewrite': 7})
        self.assertEqual((profile['source'], profile['profile'], profile['stage'], profile['mode']),
                         (PASS, PROFILE, 'pre_codegen', 'rewrite'))
        self.assertEqual(profile['includes'], [RULES])
        self.assertEqual(Counter(r['kind'] for r in profile['rules']), {'check': 2, 'rewrite': 2})
        checked = {'CheckedArrayIndex': 2, 'CheckedArraySlice': 3}
        self.assertEqual({r['match']['op'] for r in profile['rules'] if r['kind'] == 'check'}, set(checked))
        self.assertEqual({r['match']['op'] for r in profile['rules'] if r['kind'] == 'rewrite'}, set(checked))
        lines = (root / RULES).read_text(encoding='utf-8').splitlines()
        for rule in profile['rules']:
            self.assertEqual(rule['source'], RULES)
            self.assertEqual(lines[rule['line'] - 1].split()[:2], [rule['kind'], rule['name']])
            matching = rule['match']
            self.assertEqual(len(matching['args']), checked[matching['op']])
            if rule['kind'] == 'rewrite':
                self.assertEqual(rule['replace']['op'], matching['op'].replace('Checked', 'Proved'))
                self.assertEqual(rule['replace']['args'], matching['args'])
                self.assertIn('constant_bounds_in_range', rule['conditions'])

    def test_preview_is_never_a_proof(self):
        root = self.copy()
        graph = {"op": "CopyScalar", "args": [
            {"op": "CopyScalar", "args": [{"op": "Value", "args": [], "value": 42}]}
        ]}
        matches = preview.inspect(graph, self.compile_package(root))
        self.assertEqual(len(matches), 1)
        match = matches[0]
        self.assertEqual(match['status'], 'UNVERIFIED_DO_NOT_PUBLISH')
        self.assertEqual(match['rule'], 'scalar_copy_collapse')
        self.assertEqual(match['candidate_after']['args'][0]['op'], 'Value')
        self.assertIn('typed_hir', match['requires_unproven'])

    def test_fixed_array_preview_is_not_admission(self):
        # Abstract graph shape alone tells us nothing about this bound.
        graph = {'op': 'CheckedArrayIndex', 'args': [
            {'op': 'Value', 'args': []},
            {'op': 'Value', 'args': [], 'value': -1},
        ]}
        candidate, = preview.inspect(graph, self.compile_package(self.copy()))
        self.assertEqual(candidate['status'], 'UNVERIFIED_DO_NOT_PUBLISH')
        self.assertEqual(candidate['candidate_after']['op'], 'ProvedArrayIndex')
        self.assertIn('constant_bounds_in_range', candidate['requires_unproven'])

    def test_profile_clause_grammar(self):
        for incorrect in [PROFILE_CLAUSE * 2,
                          PROFILE_CLAUSE.replace(';', ''),
                          PROFILE_CLAUSE.replace(PROFILE, f'"{PROFILE}"')]:
            with self.subTest(clause=incorrect):
                root = self.copy()
                self.replace(root, PASS, PROFILE_CLAUSE, incorrect)
                self.must_fail(root)
        root = self.copy()
        self.replace(root, RULES, f'patterns {PROFILE};\n', f'patterns {PROFILE};\n{PROFILE_CLAUSE}')
        self.must_fail(root)

    def test_rejects_unauthorized_profiles(self):
        for path, old, new, diagnostic in [
            (PASS, PROFILE_CLAUSE, 'profile c11_fixed_array_v999;\n', 'unsupported executable profile'),
            (PASS, 'stage pre_codegen', 'stage post_ownership', 'requires pre_codegen rewrite mode'),
            (PASS, 'mode rewrite', 'mode verify', 'requires pre_codegen rewrite mode'),
            ('40-canonical.mmg', 'include "canonical-types.mmgi";',
             PROFILE_CLAUSE + 'include "canonical-types.mmgi";', 'duplicate executable profile'),
        ]:
            with self.subTest(path=path, replacement=new):
                root = self.copy()
                self.replace(root, path, old, new)
                self.must_fail(root, diagnostic)

    def test_profile_must_be_explicit_for_header_generation(self):
        # The standalone preview may still open a package with no executable
        # profile at all, but it can never produce a linked table from it.
        root = self.copy()
        (root / PASS).unlink()
        (root / RULES).unlink()
        plan = self.compile_package(root)
        with self.assertRaisesRegex(mmgc.SpecError, 'without executable profile'):
            mmgc.emit_header(plan)

    def test_rejects_weakened_profile_proofs(self):
        for old, new in [
            ('exact_i64_bounds, emitted_bounds_check', 'emitted_bounds_check'),
            ('bounded_checked_i64_evaluation, ', ''),
            ('constant_bounds_in_range, no_hidden_effect', 'no_hidden_effect'),
            ('constant_bounds_in_range, no_hidden_effect', 'bounds_proven_or_checked, no_hidden_effect'),
            ('constant_bounds_in_range, no_hidden_effect', 'constant_bounds_in_range, author_says_safe, no_hidden_effect'),
        ]:
            with self.subTest(replacement=new):
                root = self.copy()
                self.replace(root, RULES, old, new)
                self.must_fail(root)

    def test_rejects_profile_rewrites_that_change_meaning(self):
        for old, new in [
            ('replace ProvedArraySlice($base, $lo, $hi)', 'replace ProvedArraySlice($base, $hi, $lo)'),
            ('replace ProvedArrayIndex($base, $index)', 'replace ProvedArrayIndex($base, $base)'),
            ('replace ProvedArrayIndex($base, $index)', 'replace ProvedArrayIndex($base)'),
            ('replace ProvedArrayIndex($base, $index)', 'replace Index($base, $index)'),
            ('replace ProvedArrayIndex($base, $index)', 'replace ProvedArrayIndex(CopyScalar($base), $index)'),
        ]:
            with self.subTest(replacement=new):
                root = self.copy()
                self.replace(root, RULES, old, new)
                self.must_fail(root)

    def test_rejects_executable_operators_in_preview_pass(self):
        root = self.copy()
        self.replace(root, 'canonical-memory.mmgi', 'replace BoundsValidated($place)',
                     'replace BoundsValidated(ProvedArrayIndex($place, "forged"))')
        self.must_fail(root)

    def test_generated_parser_emits_reproducible_header(self):
        if self.parser is None:
            self.skipTest('ON1X_MEMGRAPH_TEST_PARSER is not set')
        root = self.copy()
        plan = self.compile_package(root)
        header = mmgc.emit_header(plan)
        self.assertEqual(header, mmgc.emit_header(self.compile_package(root)))
        self.assertIn('std::array<Rule, 4>', header)
        self.assertIn('"static_array_index"', header)
        self.assertIn('"static_array_slice"', header)
        self.assertNotIn('scalar_copy_collapse', header)

        # A failed generation must leave previous artifacts untouched.
        out = root / 'rules.hpp'
        manifest = root / 'manifest.json'
        out.write_text(header, encoding='utf-8')
        manifest.write_text('already published\n', encoding='utf-8')
        self.replace(root, PASS, PROFILE_CLAUSE, 'profile c11_fixed_array_v999;\n')
        result = subprocess.run([sys.executable, ROOT / 'tools/mmgc.py', '--root', root,
                                 '--parser', self.parser, '--header', out, '--manifest', manifest],
                                capture_output=True, encoding='utf-8', timeout=20)
        self.assertEqual(result.returncode, 1, result)
        self.assertIn('unsupported executable profile', result.stderr)
        self.assertEqual(out.read_text(encoding='utf-8'), header)
        self.assertEqual(manifest.read_text(encoding='utf-8'), 'already published\n')

    def test_rejects_unsafe_include(self):
        root = self.copy()
        self.replace(root, '00-types.mmg', '"types.mmgi"', '"../types.mmgi"')
        self.must_fail(root, 'unsafe or malformed include')

    def test_rejects_symlink_include(self):
        root = self.copy()
        alias = root / RULES
        target = alias.with_suffix('.bak')
        alias.rename(target)
        alias.symlink_to(target.name)
        self.must_fail(root, 'regular sibling')

    def test_rejects_missing_guard(self):
        root = self.copy()
        self.replace(root, 'canonical-memory.mmgi', 'copy_safe_scalar, copy_no_trap, no_hidden_effect',
                     'copy_safe_scalar, copy_no_trap')
        self.must_fail(root, 'needs typed_hir and no_hidden_effect')

    def test_rejects_copying_capture(self):
        root = self.copy()
        self.replace(root, 'canonical-memory.mmgi',
                     'replace CopyScalar($value)',
                     'replace Index($value, $value)')
        self.must_fail(root, 'every capture exactly once')

    def test_rejects_unimplemented_guard(self):
        root = self.copy()
        self.replace(root, 'ffi.mmgi', 'exact_cdecl_abi, target_revision_pinned',
                     'auto_approve_ffi, target_revision_pinned')
        self.must_fail(root, 'unsupported check condition')

    def test_rejects_forged_preservation(self):
        root = self.copy()
        self.replace(root, 'canonical-types.mmgi',
                     'ffi, traps, provenance;', 'ffi, traps;')
        self.must_fail(root, 'all nine preservation')

    def test_rejects_verify_pass_rewrite(self):
        root = self.copy()
        self.replace(root, '40-canonical.mmg', 'mode rewrite', 'mode verify')
        self.must_fail(root, 'rewrite in verify-only pass')

    def test_rejects_foreign_pass_mismatch(self):
        root = self.copy()
        self.replace(root, 'ffi.mmgi', 'patterns ffi;', 'patterns canonical;')
        self.must_fail(root, 'pattern group does not match')

    def test_rejects_budget_mismatch(self):
        root = self.copy()
        self.replace(root, 'ffi.mmgi', 'steps 1048576', 'steps 1048575')
        self.must_fail(root, 'budget differs')

    def test_rejects_new_orphan_file(self):
        root = self.copy()
        shutil.copy2(root / 'ffi.mmgi', root / 'unused.mmgi')
        self.must_fail(root, 'orphan or unreferenced')

    def test_rejects_rewrite_that_drops_ownership_operand(self):
        root = self.copy()
        self.replace(root, 'canonical-owners.mmgi',
                     'replace OwnerValidated($owner)',
                     'replace OwnerValidated(CopyScalar($another))')
        self.must_fail(root, 'every capture exactly once')

    def test_unknown_op_is_error(self):
        root = self.copy()
        self.replace(root, 'types.mmgi', 'match Cast($value, $type)',
                     'match TrustAllFFI($value, $type)')
        self.must_fail(root, 'unknown graph operator')


if __name__ == '__main__':
    unittest.main(verbosity=2)
