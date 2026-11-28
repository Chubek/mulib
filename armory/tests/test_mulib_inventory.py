#!/usr/bin/env python3
"""Regression checks for the dependency-free MuLL bridge freshness check."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'check_mulib_inventory', Path(__file__).resolve().parents[1] / 'scripts/check_mulib_inventory.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class InventoryTests(unittest.TestCase):
    def setUp(self):
        parent = '/tmp/opencode' if Path('/tmp/opencode').is_dir() else None
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.include = self.root / 'kernels'
        self.bindings = self.root / 'bindings'
        self.include.mkdir()
        self.bindings.mkdir()
        self.header = self.include / 'mulib-example.h'
        self.header.write_text('int mulib_example(void);\n')
        self.discover_header = self.include / 'mulib-new.h'
        self.discover_header.write_text('int mulib_new(void);\n')
        self.depend = self.include / 'detail/value.inc'
        self.depend.parent.mkdir()
        self.depend.write_text('#define EXAMPLE 1\n')
        raw = (checker.LIBRARIES + b'{"example","mulib-example.h",mb_symbols_0,1},\n};\n')
        policy = {'headers': {self.header.name: {'functions': ['mulib_example']}},
                  'surface_sha256': hashlib.sha256(raw).hexdigest()}
        policy_bytes = json.dumps(policy).encode()
        (self.bindings / 'LegacyExports.json').write_bytes(policy_bytes)
        artifacts = {
            'Generated.inc': raw.replace(b'};\n', b'{"new","mulib-new.h",NULL,0},\n};\n'),
            'Implementation.c': b'implementation\n',
        }
        for name, text in artifacts.items():
            (self.bindings / name).write_bytes(text)
        self.inventory = {
            'headers': [self.header.name, self.discover_header.name],
            'libraries': 2, 'functions': 1, 'declared_functions': 2,
            'modules': {
                'example': {'header': self.header.name, 'profile': 'legacy-raw',
                            'functions': ['mulib_example'], 'declarations': ['mulib_example']},
                'new': {'header': self.discover_header.name, 'profile': 'discovery-only',
                        'functions': [], 'declarations': ['mulib_new']},
            },
            'fingerprint': checker.header_fingerprint(self.include),
            'legacy_policy': hashlib.sha256(policy_bytes).hexdigest(),
            'artifacts': {name: hashlib.sha256(text).hexdigest() for name, text in artifacts.items()},
        }
        self.save()

    def save(self):
        (self.bindings / 'Inventory.json').write_text(json.dumps(self.inventory))

    def check(self):
        return checker.check_inventory(self.include, self.bindings)

    def test_current_snapshot(self):
        self.assertEqual(self.check(), (2, 2, 1))

    def test_new_header(self):
        (self.include / 'mulib-added.h').write_text('int mulib_added(void);\n')
        with self.assertRaisesRegex(ValueError, 'missing=.*mulib-added.h'):
            self.check()

    def test_changed_header(self):
        self.header.write_text('int mulib_example(int argument);\n')
        with self.assertRaisesRegex(ValueError, 'Header inputs changed'):
            self.check()

    def test_changed_detail_dependency(self):
        self.depend.write_text('#define EXAMPLE 2\n')
        with self.assertRaisesRegex(ValueError, 'Header inputs changed'):
            self.check()

    def test_missing_library(self):
        self.inventory['modules'] = {}
        self.save()
        with self.assertRaisesRegex(ValueError, 'Library registrations'):
            self.check()

    def test_inconsistent_functions(self):
        self.inventory['modules']['example']['declarations'] = []
        self.save()
        with self.assertRaisesRegex(ValueError, 'Function declarations'):
            self.check()

    def test_discovery_is_not_callable_policy(self):
        self.inventory['modules']['new']['functions'] = ['mulib_new']
        self.inventory['functions'] = 2
        self.save()
        with self.assertRaisesRegex(ValueError, 'Legacy function registrations'):
            self.check()

    def test_changed_policy(self):
        policy = self.bindings / 'LegacyExports.json'
        policy.write_bytes(policy.read_bytes() + b'\n')
        with self.assertRaisesRegex(ValueError, 'Legacy export policy changed'):
            self.check()

    def test_rehash_cannot_hide_changed_wrapper_surface(self):
        artifact = self.bindings / 'Generated.inc'
        contents = artifact.read_bytes().replace(b'mb_symbols_0,1', b'mb_symbols_0,2')
        artifact.write_bytes(contents)
        self.inventory['artifacts']['Generated.inc'] = hashlib.sha256(contents).hexdigest()
        self.save()
        with self.assertRaisesRegex(ValueError, 'C legacy surface differs'):
            self.check()

    def test_rehash_cannot_hide_nonempty_discovery_table(self):
        artifact = self.bindings / 'Generated.inc'
        contents = artifact.read_bytes().replace(b'NULL,0', b'mb_symbols_0,1')
        artifact.write_bytes(contents)
        self.inventory['artifacts']['Generated.inc'] = hashlib.sha256(contents).hexdigest()
        self.save()
        with self.assertRaisesRegex(ValueError, 'C discovery rows'):
            self.check()

    def test_changed_artifact(self):
        (self.bindings / 'Generated.inc').write_text('old bridge\n')
        with self.assertRaisesRegex(ValueError, 'Generated.inc does not match'):
            self.check()


if __name__ == '__main__':
    unittest.main()
