#!/usr/bin/env python3
"""Regression tests for additive media inventory generation; no repository writes."""
import importlib.util
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
spec = importlib.util.spec_from_file_location("media_inventory", ROOT / "tools/media_inventory.py")
inventory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)


class InventoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.records = inventory.inventory()

    def test_exact_inventory_and_contracts(self):
        self.assertEqual(len(self.records), 194)
        self.assertEqual(len({r["header"] for r in self.records}), 194)
        self.assertEqual(sum(r["header"] not in inventory.SUPPLEMENTAL
                             for r in self.records), 193)
        self.assertEqual(set(inventory.SUPPLEMENTAL), {"mulib-audio-core.h"})
        self.assertFalse({r["header"] for r in self.records} & inventory.SUPPORT)
        self.assertEqual({r["header"][6:-2] for r in self.records}, set(inventory.CONTRACTS))

    def test_source_dependencies(self):
        for record in self.records:
            source = (ROOT / "kernels" / record["header"]).read_text()
            expected = sorted(set(re.findall(
                r'^\s*#\s*include\s*["<](mulib-[^">]+\.h)[">]', source, re.M)))
            self.assertEqual(record["dependencies"], expected)

    def test_manual_surfaces_and_references(self):
        documents = {}
        for record in self.records:
            path = ROOT / "manual" / (record["header"][:-2] + ".md")
            text = inventory.document(record, self.records)
            self.assertIn("not safe On1x admitted", text)
            self.assertIn(inventory.CONTRACTS[record["header"][6:-2]], text)
            self.assertEqual(text.count("<!-- manual-reference -->"), 1)
            self.assertEqual(text.count("<!-- /manual-reference -->"), 1)
            self.assertIn("static inline ", text)
            documents[path] = text
        self.assertEqual(inventory.manuals.check_links(documents), [])

    def test_additive_manifest_preserves_existing_and_project_files(self):
        existing = (
            "name: Fixture\nmodule_count: 1\nbounded_module_count: 0\n"
            "implemented_module_count: 1\nmodules:\n"
            "- name: existing\n  header: kernels/mulib-existing.h\n"
            "  macro: MULIB_IMPL_EXISTING\n  status: implemented\n"
            "  purpose: User-owned description.\n"
        )
        project = "project_files:\n- file: keep/me\n  purpose: Keep byte-for-byte.\n"
        generated = inventory.manifest_with_additions(existing + project, self.records)
        self.assertTrue(generated.endswith(project))
        self.assertIn("module_count: 195\n", generated)
        self.assertIn("bounded_module_count: 194\n", generated)
        self.assertIn("implemented_module_count: 1\n", generated)
        self.assertIn(existing[existing.index("- name: existing"):], generated)
        self.assertEqual(generated.count("  implementation: static-inline\n"), 194)
        self.assertEqual(inventory.manifest_with_additions(generated, self.records), generated)

    def test_collision_is_not_overwritten(self):
        fixture = (
            "module_count: 1\nbounded_module_count: 0\nmodules:\n"
            "- name: media-core\n  header: kernels/mulib-media-core.h\n"
            "  macro: MULIB_IMPL_USER_CORE\n  status: implemented\nproject_files:\n"
        )
        with self.assertRaisesRegex(ValueError, "refuse to replace"):
            inventory.manifest_with_additions(fixture, self.records)

    def test_duplicate_and_name_collisions_are_rejected(self):
        base = "module_count: 2\nbounded_module_count: 0\nmodules:\n"
        block = "- name: x\n  header: kernels/mulib-x.h\n  status: implemented\n"
        with self.assertRaisesRegex(ValueError, "duplicate"):
            inventory.manifest_with_additions(base + block + block + "project_files:\n", self.records)
        other = "- name: media-core\n  header: kernels/mulib-user.h\n  status: implemented\n"
        with self.assertRaisesRegex(ValueError, "module-name collision"):
            inventory.manifest_with_additions(base + other + "project_files:\n", self.records)

    def test_missing_counts_and_boundary_rejected(self):
        for fixture in ["modules:\nproject_files:\n", "module_count: 0\nmodules:\n"]:
            with self.assertRaises(ValueError):
                inventory.manifest_with_additions(fixture, self.records)

    def test_signatures_include_multiline_declarators(self):
        source = (
            "/* static inline bool not_a_function(void) {} */\n"
            "static inline bool f(\nconst int *p, size_t cap)\n{ return p && cap; }\n"
            "static inline void g(void (*callback)(int)) { callback(1); }\n"
        )
        self.assertEqual(inventory.signatures(source),
                         "static inline bool f( const int *p, size_t cap);\n"
                         "static inline void g(void (*callback)(int));")


if __name__ == "__main__":
    unittest.main()
