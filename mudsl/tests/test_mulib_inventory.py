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
        self.include = self.root / 'include'
        self.bindings = self.root / 'bindings'
        self.include.mkdir()
        self.bindings.mkdir()
        self.header = self.include / 'mulib-example.h'
        self.header.write_text('int mulib_example(void);\n')
        artifacts = {'Generated.inc': 'generated bridge\n', 'Implementation.c': 'implementation\n'}
        for name, text in artifacts.items():
            (self.bindings / name).write_text(text)
        self.inventory = {
            'headers': [self.header.name], 'libraries': 1, 'functions': 1,
            'modules': {'example': {'header': self.header.name, 'functions': ['mulib_example']}},
            'fingerprint': hashlib.sha256(self.header.read_bytes()).hexdigest(),
            'artifacts': {name: hashlib.sha256(text.encode()).hexdigest() for name, text in artifacts.items()},
        }
        self.save()

    def save(self):
        (self.bindings / 'Inventory.json').write_text(json.dumps(self.inventory))

    def check(self):
        return checker.check_inventory(self.include, self.bindings)

    def test_current_snapshot(self):
        self.assertEqual(self.check(), (1, 1, 1))

    def test_new_header(self):
        (self.include / 'mulib-new.h').write_text('int mulib_new(void);\n')
        with self.assertRaisesRegex(ValueError, 'missing=.*mulib-new.h'):
            self.check()

    def test_changed_header(self):
        self.header.write_text('int mulib_example(int argument);\n')
        with self.assertRaisesRegex(ValueError, 'Header APIs changed'):
            self.check()

    def test_missing_library(self):
        self.inventory['modules'] = {}
        self.save()
        with self.assertRaisesRegex(ValueError, 'Library registrations'):
            self.check()

    def test_inconsistent_functions(self):
        self.inventory['modules']['example']['functions'] = []
        self.save()
        with self.assertRaisesRegex(ValueError, 'Function registrations'):
            self.check()

    def test_changed_artifact(self):
        (self.bindings / 'Generated.inc').write_text('old bridge\n')
        with self.assertRaisesRegex(ValueError, 'Generated.inc does not match'):
            self.check()


if __name__ == '__main__':
    unittest.main()
