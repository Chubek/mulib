"""Regression tests for the generated, discovery-only KernelBind inventory."""

import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / "tools/extend-kernelbind.py"
SYNC = ROOT / "tools/sync_inventory.py"
CATALOG = Path("on1x/contracts-exotract/kernel-auto.json")


def entry(name, *, status="implemented", dependencies="[]", extra=""):
    text = f"- name: {name}\n  header: include/mulib-{name}.h\n"
    if status != "reserved":
        text += f"  manual: manual/mulib-{name}.md\n"
    if status == "implemented":
        macro = re.sub(r"[^A-Z0-9]", "_", name.upper())
        text += f"  macro: MULIB_IMPL_{macro}\n"
    if status == "bounded":
        text += "  implementation: static-inline\n"
    return text + f"  dependencies: {dependencies}\n  status: {status}\n" + extra


class ExtendKernelBind(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="mulib-kernel-auto-")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.manifest = self.root / "MANIFEST.yaml"
        self.catalog = self.root / CATALOG
        self.catalog.parent.mkdir(parents=True)
        (self.root / "include").mkdir()
        (self.root / "manual").mkdir()

    def run_tool(self, *flags, expected=0, root=None):
        args = [sys.executable, str(TOOL), "--root", str(root or self.root), *flags]
        result = subprocess.run(args, cwd=self.root, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, expected, result.stderr)
        return result

    def set_manifest(self, *entries, headers=()):
        for name in headers:
            (self.root / "include" / f"mulib-{name}.h").write_text(
                "#error not a declaration or build test\n", encoding="utf-8")
            self.write_manual(name)
        self.manifest.write_text(
            "name: Fixture\nmodules:\n" + "".join(entries)
            + "project_files:\n- name: not-a-kernel\n  purpose: ignored\n", encoding="utf-8")

    def write_manual(self, name):
        path = self.root / "manual" / f"mulib-{name}.md"
        path.write_text(f"# {name}\n", encoding="utf-8")
        return path

    def run_sync(self, *, expected=0):
        result = subprocess.run([sys.executable, str(SYNC), "--root", str(self.root), "--metadata-only"],
                                cwd=self.root, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, expected, result.stderr)
        return result

    def test_deterministic_write_and_nonwriting_check(self):
        implemented = entry("normal", dependencies="\n    - bounded",
                            extra="  optional_dependency: OpenSSL Crypto 1.1.1 or newer\n")
        bounded = entry("bounded", status="bounded")
        reserved = entry("reserved", status="reserved")
        self.set_manifest(implemented, bounded, reserved, headers=("normal", "bounded"))
        original_manifest = self.manifest.read_bytes()
        original_headers = {p: p.read_bytes() for p in (self.root / "include").iterdir()}
        self.assertIn("missing or stale catalog", self.run_tool("--check", expected=1).stderr)
        self.assertFalse(self.catalog.exists())
        self.run_tool()
        expected = {
            "schema": "on1x.kernel-auto/v1",
            "safe_export": False,
            "modules": {
                "bounded": {"header": "include/mulib-bounded.h", "status": "bounded",
                            "implementation": "static-inline", "dependencies": []},
                "normal": {"header": "include/mulib-normal.h", "status": "implemented",
                            "macro": "MULIB_IMPL_NORMAL", "dependencies": ["bounded"],
                           "optional_dependency": "OpenSSL Crypto 1.1.1 or newer"},
            },
        }
        original = self.catalog.read_bytes()
        self.assertEqual(json.loads(original), expected)
        state = self.catalog.stat()
        self.run_tool()
        self.run_tool("--check")
        after = self.catalog.stat()
        self.assertEqual((after.st_ino, after.st_mtime_ns), (state.st_ino, state.st_mtime_ns))
        self.assertEqual(self.manifest.read_bytes(), original_manifest)
        self.assertEqual({p: p.read_bytes() for p in original_headers}, original_headers)
        # Catalog key order is independent of MANIFEST record order.
        self.set_manifest(reserved, bounded, implemented)
        self.run_tool("--check")
        self.assertEqual(self.catalog.read_bytes(), original)

    def test_future_entries_and_stale_catalog(self):
        first = entry("first")
        self.set_manifest(first, headers=("first",))
        self.run_tool()
        original = self.catalog.read_bytes()
        # Preserve exact canonical names that aren't native On1x identifiers.
        self.set_manifest(first, entry("new-kernel", dependencies="[first]"),
                          entry("new_inline", status="bounded"), entry("2dshape"),
                          entry("custom+tree"),
                          headers=("new-kernel", "new_inline", "2dshape", "custom+tree"))
        self.assertIn("missing or stale catalog", self.run_tool("--check", expected=1).stderr)
        self.assertEqual(self.catalog.read_bytes(), original)
        self.run_tool()
        modules = json.loads(self.catalog.read_bytes())["modules"]
        self.assertEqual(list(modules), ["2dshape", "custom+tree", "first", "new-kernel", "new_inline"])
        self.assertEqual(modules["new-kernel"]["dependencies"], ["first"])
        self.run_tool("--check")

        changed = self.catalog.read_bytes().replace(b'"safe_export": false', b'"safe_export": true')
        self.catalog.write_bytes(changed)
        self.run_tool("--check", expected=1)
        self.assertEqual(self.catalog.read_bytes(), changed)
        self.run_tool()
        self.run_tool("--check")

    def test_ambiguous_source_aliases_do_not_publish(self):
        first = entry("first")
        self.set_manifest(first, headers=("first",))
        self.run_tool()
        original = self.catalog.read_bytes()
        for name, collision in (("some-kernel", "some_kernel"),
                                ("b+tree", "b_plus_tree"), ("2dshape", "k_2dshape")):
            with self.subTest(name=name):
                self.set_manifest(first, entry(name), entry(collision, status="bounded"),
                                  headers=(name, collision))
                result = self.run_tool(expected=1)
                self.assertIn(f"ambiguous manifest alias: mulib.{collision}", result.stderr)
                self.assertIn(name, result.stderr)
                self.assertIn(collision, result.stderr)
                self.assertEqual(self.catalog.read_bytes(), original)
                for extra in (name, collision):
                    (self.root / "include" / f"mulib-{extra}.h").unlink()

    def test_unregistered_header_and_static_data(self):
        self.set_manifest(entry("normal"), headers=("normal",))
        self.run_tool()
        original = self.catalog.read_bytes()
        header = self.root / "include/mulib-embedded.h"
        header.write_text('static const char source[] = "embedded data";\n')
        for flags in ((), ("--check",)):
            self.assertIn("unregistered kernel header", self.run_tool(*flags, expected=1).stderr)
            self.assertEqual(self.catalog.read_bytes(), original)
        data_entry = entry("embedded", status="bounded").replace("static-inline", "static-data")
        self.set_manifest(entry("normal"), data_entry)
        self.write_manual("embedded")
        self.run_tool()
        self.run_tool("--check")
        catalog = json.loads(self.catalog.read_bytes())
        self.assertFalse(catalog["safe_export"])
        self.assertEqual(catalog["modules"]["embedded"], {
            "header": "include/mulib-embedded.h", "status": "bounded",
            "implementation": "static-data", "dependencies": [],
        })
        self.assertEqual(header.read_text(), 'static const char source[] = "embedded data";\n')

    def test_reserved_headers_are_registered_but_not_exported(self):
        self.set_manifest(entry("normal"), entry("reserved", status="reserved"),
                          headers=("normal", "reserved"))
        self.run_tool()
        original = self.catalog.read_bytes()
        self.assertEqual(set(json.loads(original)["modules"]), {"normal"})
        self.run_tool("--check")
        (self.root/"include/mulib-reserved.h").unlink()
        self.run_tool("--check")
        self.run_tool()
        self.assertEqual(self.catalog.read_bytes(), original)
        for active, reserved, diagnostic in (
            (entry("normal", dependencies="[reserved]"),
             entry("reserved", status="reserved"), "reserved MANIFEST dependency"),
            (entry("normal"), entry("reserved", status="reserved", dependencies="[normal]"),
             "reserved MANIFEST module cannot declare dependencies"),
        ):
            with self.subTest(diagnostic=diagnostic):
                self.set_manifest(active, reserved)
                self.assertIn(diagnostic, self.run_tool(expected=1).stderr)
                self.assertEqual(self.catalog.read_bytes(), original)

    def test_malformed_manifest_preserves_existing_catalog(self):
        base = entry("valid")
        self.set_manifest(base, headers=("valid",))
        self.run_tool()
        original = self.catalog.read_bytes()
        cases = [
            (base + base, "duplicate module name"),
            (base + "- name: ../../escape\n", "malformed module name"),
            (base + "- name: [not-a-scalar]\n", "malformed module name"),
            (base + "-  name: hidden\n", "malformed module name"),
            (base + "name: missing-dash\n", "malformed module name"),
            (base + "  name: not-valid\n", "duplicate field"),
            (base + "  status: implemented\n", "duplicate field"),
            (base.replace("status: implemented", "status: unknown"), "unsupported status"),
            (base.replace("MULIB_IMPL_VALID", "$FORGED"), "implementation metadata"),
            (base.replace("include/mulib-valid.h", "../mulib-valid.h"), "noncanonical module header"),
            (base.replace("dependencies: []", "dependencies: [missing]"), "unknown dependency"),
            (base.replace("dependencies: []", "dependencies: [valid, valid]"), "dependencies layout"),
            (base.replace("manual/mulib-valid.md", "manual/mulib-absent.md"), "missing/escaping MuLIB input"),
            (base.replace("  manual: manual/mulib-valid.md\n", ""), "MANIFEST field: manual"),
            (base + entry("missing"), "missing/escaping MuLIB input"),
            (base + "modules:\n" + base, "duplicate modules section"),
        ]
        for text, message in cases:
            with self.subTest(message=message, record=text[len(base):]):
                self.set_manifest(text)
                result = self.run_tool(expected=1)
                self.assertIn(message, result.stderr)
                self.assertEqual(self.catalog.read_bytes(), original)
                self.assertEqual(list(self.catalog.parent.iterdir()), [self.catalog])

    def test_metadata_only_workflow_reaches_shared_inventory(self):
        self.manifest.write_text(
            "name: Fixture\nmodule_count: 2\nimplemented_module_count: 0\nmodules:\n"
            + entry("shared-inline", status="bounded", dependencies="\n    - stale")
            + entry("shared-data", status="bounded").replace("static-inline", "static-data")
            + "project_files:\n- file: handwritten.md\n  purpose: Preserve this entry.\n")
        headers = {
            "shared-inline": ('#include "mulib-shared-data.h"\n'
                              'static inline int value(void) { return metadata[0]; }\n'),
            "shared-data": "static const int metadata[] = { 7 };\n",
            "fixture+kernel": ('/** @brief Future kernel. */\n#include "mulib-shared-inline.h"\n'
                               "int future(void);\n  #if defined MULIB_IMPL_FIXTURE_KERNEL\n"
                               "int future(void) { return value(); }\n#endif\n"),
            "future-reserved": "/* intentionally reserved */\n",
        }
        for name, source in headers.items():
            (self.root / "include" / f"mulib-{name}.h").write_text(source)
            if name not in ("fixture+kernel", "future-reserved"):
                self.write_manual(name)
        self.assertFalse((self.root / "tests").exists())
        original = self.manifest.read_bytes()
        failure = self.run_sync(expected=1)
        self.assertIn("manual/mulib-fixture+kernel.md", failure.stderr)
        self.assertEqual(self.manifest.read_bytes(), original)
        self.assertFalse(self.catalog.exists())

        self.write_manual("fixture+kernel")
        self.run_sync()
        self.run_tool("--check")
        spec = importlib.util.spec_from_file_location("fixture_contracts", ROOT / "on1x/tools/on1x-contracts.py")
        contracts = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(contracts)
        ordinary = contracts.manifest_inventory(self.root)
        strict = contracts.manifest_inventory(self.root, registration=True)
        contracts.KERNEL_AUTO_CATALOG = self.catalog
        registered, _ = contracts.kernel_auto_catalog(strict)
        self.assertEqual(set(ordinary), set(strict))
        self.assertEqual(set(registered), set(headers) - {"future-reserved"})
        self.assertEqual(registered["fixture+kernel"]["macro"], "MULIB_IMPL_FIXTURE_KERNEL")
        self.assertEqual(registered["fixture+kernel"]["dependencies"], ["shared-inline"])
        self.assertEqual(registered["shared-inline"]["dependencies"], ["shared-data"])
        self.assertEqual(registered["shared-inline"]["implementation"], "static-inline")
        self.assertEqual(registered["shared-data"]["implementation"], "static-data")
        self.assertNotIn("macro", registered["shared-data"])
        self.assertFalse(json.loads(self.catalog.read_bytes())["safe_export"])
        self.assertIn("bounded_module_count: 2\n", self.manifest.read_text())
        self.assertIn("purpose: Preserve this entry.", self.manifest.read_text())
        self.assertIn("future-reserved", self.manifest.read_text())
        self.assertNotIn("    - stale", self.manifest.read_text())

        # Idempotence without requiring either C fixture to exist or be valid.
        (self.root / "tests").mkdir()
        declarations = self.root / "tests/declarations.c"
        declarations.write_bytes(b"\xff")
        before = (self.manifest.read_bytes(), self.catalog.read_bytes())
        self.run_sync()
        self.assertEqual((self.manifest.read_bytes(), self.catalog.read_bytes()), before)
        self.assertEqual(declarations.read_bytes(), b"\xff")
        self.assertFalse((self.root / "tests/implementation.c").exists())

        with (self.root / "include/mulib-fixture+kernel.h").open("a") as header:
            header.write('#if defined(WITH_RESERVED)\n#include "mulib-future-reserved.h"\n#endif\n')
        self.assertIn("reserved MANIFEST dependency", self.run_sync(expected=1).stderr)
        self.assertEqual((self.manifest.read_bytes(), self.catalog.read_bytes()), before)

    def test_publishing_additions_follow_canonical_header_names(self):
        self.manifest.write_text("name: Fixture\nmodules:\nproject_files:\n")
        name = "2d+data"
        (self.root / "include" / f"mulib-{name}.h").write_text("static const int data[] = {1};\n")
        additions = self.root / "MANIFEST.yaml.additions"
        additions.write_text("modules:\n" + entry(name, status="bounded").replace("static-inline", "static-data"))
        before = self.manifest.read_bytes()
        self.assertIn(f"manual/mulib-{name}.md", self.run_sync(expected=1).stderr)
        self.assertEqual(self.manifest.read_bytes(), before)
        self.assertTrue(additions.is_file())
        self.assertFalse(self.catalog.exists())
        self.write_manual(name)
        self.run_sync()
        self.assertFalse(additions.exists())
        self.run_tool("--check")
        self.assertEqual(list(json.loads(self.catalog.read_bytes())["modules"]), [name])

    def test_bounded_inputs_and_symlinked_catalog(self):
        self.manifest.write_bytes(b"#" * (8 * 1024 * 1024 + 1))
        self.assertIn("bounded input", self.run_tool(expected=1).stderr)
        self.assertFalse(self.catalog.exists())

        self.set_manifest(entry("valid"), headers=("valid",))
        target = self.root / "user-file"
        target.write_bytes(b"preserve me")
        self.catalog.symlink_to(target)
        self.assertIn("not a regular catalog file", self.run_tool(expected=1).stderr)
        self.assertEqual(target.read_bytes(), b"preserve me")
        self.assertTrue(self.catalog.is_symlink())

    def test_live_manifest_coverage(self):
        self.run_tool("--check", root=ROOT)
        text = (ROOT / "MANIFEST.yaml").read_text(encoding="utf-8")
        entries = text.split("\nmodules:\n", 1)[1].split("\nproject_files:", 1)[0]
        expected = {}
        for record in re.split(r"(?m)^- name: ", entries)[1:]:
            name, _, body = record.partition("\n")
            status = re.search(r"(?m)^  status: ([a-z-]+)$", body)[1]
            if status in ("implemented", "bounded"):
                expected[name] = status
        catalog = json.loads((ROOT / CATALOG).read_bytes())
        self.assertEqual(expected, {name: item["status"] for name, item in catalog["modules"].items()})
        for name in ("imtui", "imtui-core", "termscript", "termscript-runtime", "termdb",
                     "termfunc", "sshconnection", "sshcrypto", "sshserver", "2dgeom", "b+tree",
                     "cowb+tree"):
            with self.subTest(name=name):
                self.assertIn(name, catalog["modules"])
        self.assertEqual(catalog["modules"]["sshcrypto"]["optional_dependency"],
                         "OpenSSL Crypto 1.1.1 or newer")
        self.assertEqual({p.relative_to(ROOT).as_posix() for p in (ROOT/"include").glob("mulib-*.h")},
                         {item["header"] for item in catalog["modules"].values()})
        self.assertEqual(catalog["modules"]["container-core"]["implementation"], "static-inline")
        self.assertEqual(catalog["modules"]["qvm"]["implementation"], "static-inline")
        self.assertEqual(catalog["modules"]["qvm"]["dependencies"], ["container-core"])
        self.assertEqual(catalog["modules"]["lprowse"]["implementation"], "static-data")
        self.assertNotIn("macro", catalog["modules"]["lprowse"])

    def test_live_manual_coverage(self):
        spec = importlib.util.spec_from_file_location("registration_manuals", ROOT/"tools/manuals.py")
        manuals = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(manuals)
        records = manuals.inventory()
        self.assertEqual({record["header"] for record in records},
                         set((ROOT/"include").glob("mulib-*.h")))
        for name in ("container-core", "qvm", "lprowse"):
            record = next(r for r in records if r["header"].name == f"mulib-{name}.h")
            self.assertIsNone(record["switch"])
            self.assertTrue(record["shared"])
            self.assertTrue(record["manual"].is_file())
            self.assertTrue(record["example"].is_file())


if __name__ == "__main__":
    unittest.main()
