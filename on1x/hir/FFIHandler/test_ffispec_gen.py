#!/usr/bin/env python3
"""Exercise the generator against the real Cephyr Python/ABI installation."""

import copy
import hashlib
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest

from cephyr import Compiler

SCRIPT = Path(__file__).with_name("ffispec-gen.py")
GEN = runpy.run_path(str(SCRIPT))
ROOT, CONFIG = GEN["ROOT"], GEN["CEPHYR_CONFIG"]
INCLUDE = ROOT / "include"
REVIEW = json.loads(Path(__file__).with_name("reviewed.json").read_bytes())
CLAMP = REVIEW["clamp"]
HEADER = INCLUDE / CLAMP["header"]
SPEC = ROOT / "on1x/stdffi/mulib/clamp.ffi"
Rejection = GEN["Rejection"]


class FfiSpecGenerator(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = Compiler()
        cls.args = ["--config", CONFIG, "--stdlib=cephyr", "-I", INCLUDE, "-"]
        cls.source = '#define MULIB_IMPL_CLAMP\n#include <mulib-clamp.h>\n'

    def generate(self, *, include=INCLUDE, profile=CLAMP, target=None):
        return GEN["generate"](self.compiler, CONFIG, include, "clamp", profile, target=target)

    def make_scratch(self):
        scratch = ROOT / "build"
        scratch.mkdir(exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(temporary.cleanup)
        return Path(temporary.name)

    def modified_header(self, before, after, *, repin=False):
        directory = self.make_scratch()
        original = HEADER.read_bytes()
        self.assertIn(before, original)
        data = original.replace(before, after, 1)
        (directory / HEADER.name).write_bytes(data)
        profile = copy.deepcopy(CLAMP)
        if repin:
            profile["sha256"] = hashlib.sha256(data).hexdigest()
        return directory, profile

    def test_recipe_is_reproducible_across_cephyr_extraction_targets(self):
        for target in ("x86_64-linux-gnu", "aarch64-linux-gnu", "riscv64-linux-gnu"):
            with self.subTest(target=target):
                self.assertEqual(SPEC.read_bytes(), self.generate(target=target).encode())
        text = SPEC.read_text(encoding="ascii")
        self.assertEqual(text.count("(export "), 3)
        self.assertIn("(guard (le lo hi))", text)
        self.assertEqual(text.count("(hint private-code (backend mir-c11))"), 3)
        self.assertEqual(text.count("(hint kernel-hot (target x86_64-unknown-linux-gnu) (speculative))"), 3)
        self.assertEqual(text.count("(rewrite fold-identity (speculative))"), 3)

    def test_header_drift_is_rejected(self):
        directory, profile = self.modified_header(b"@file ", b"@file  ")
        with self.assertRaisesRegex(Rejection, "SHA-256 differs"):
            self.generate(include=directory, profile=profile)

    def test_repinned_body_with_unchanged_signature_is_rejected(self):
        directory, profile = self.modified_header(b"x < lo ? lo", b"x <= lo ? lo", repin=True)
        with self.assertRaisesRegex(Rejection, "body differs from reviewed expression"):
            self.generate(include=directory, profile=profile)

    def test_repinned_function_call_is_rejected(self):
        directory, profile = self.modified_header(
            b"return x < lo ? lo : x > hi ? hi : x;",
            b"return mulib_clamp_add_i64(x, lo);",
            repin=True,
        )
        with self.assertRaisesRegex(Rejection, "unsupported C expression"):
            self.generate(include=directory, profile=profile)

    def test_declarations_without_definitions_are_rejected(self):
        result = self.compiler.emit_interface(self.args, '#include <mulib-clamp.h>\n')
        self.assertTrue(result.ok, result.diagnostics)
        interface = GEN["read_interface"](result.output.decode())
        with self.assertRaisesRegex(Rejection, "unsupported linkage or storage"):
            GEN["interface_exports"](interface, HEADER, CLAMP["exports"])

    def test_both_cephyr_views_independently_check_header_origin(self):
        result = self.compiler.emit_interface(self.args, self.source)
        self.assertTrue(result.ok, result.diagnostics)
        text = result.output.decode()
        interface = GEN["read_interface"](text.replace(str(HEADER), str(HEADER.with_name("foreign.h"))))
        with self.assertRaisesRegex(Rejection, "not exclusive to its canonical header"):
            GEN["interface_exports"](interface, HEADER, CLAMP["exports"])
        definitions = GEN["interface_exports"](GEN["read_interface"](text), HEADER, CLAMP["exports"])
        result = self.compiler.parse_c11(self.args, self.source)
        self.assertTrue(result.ok, result.diagnostics)
        ast = json.loads(result.output)
        for entry in ast["source"]["line_map"]:
            if entry["file"] == str(HEADER):
                entry["line"] += 1
        with self.assertRaisesRegex(Rejection, "unmatched canonical AST definition"):
            GEN["ast_definitions"](ast, HEADER, definitions)

    def test_batch_distinguishes_discovery_from_review(self):
        report, status = GEN["batch"](lambda: self.compiler, CONFIG, INCLUDE, REVIEW)
        self.assertEqual(status, 0)
        self.assertEqual(report["discovered_headers"], len(list(INCLUDE.glob("mulib-*.h"))))
        ids = [entry["kernel"] for entry in report["kernels"]]
        self.assertEqual(ids, sorted(set(ids)))
        self.assertEqual(report["admitted"], report["reviewed_profiles"])
        entries = {item["kernel"]: item for item in report["kernels"]}
        self.assertEqual(entries["clamp"]["spec_sha256"], hashlib.sha256(SPEC.read_bytes()).hexdigest())
        self.assertEqual(entries["ipc-framed"]["reason"], "no-reviewed-profile")

    def test_batch_reports_reviewed_failure(self):
        def unavailable():
            raise OSError("Cephyr is unavailable")

        report, status = GEN["batch"](unavailable, CONFIG, INCLUDE, REVIEW)
        clamp = next(item for item in report["kernels"] if item["kernel"] == "clamp")
        self.assertEqual(status, 1)
        self.assertEqual((clamp["status"], clamp["reason"]), ("unavailable", "cephyr-unavailable"))
        self.assertNotIn("spec_sha256", clamp)

    def test_projection_inventory_of_real_unreviewed_headers(self):
        def project(*kernels):
            command = [sys.executable, str(SCRIPT), "--batch", "--project"]
            for kernel in kernels:
                command.extend(("--select", kernel))
            result = subprocess.run(command, capture_output=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            return result.stdout

        output = project("typecore", "encoding")
        self.assertEqual(output, project("encoding", "typecore"))
        report = json.loads(output)
        self.assertEqual((report["selected_kernels"], report["projected"], report["admitted"]), (2, 2, 0))
        encoding, typecore = report["kernels"]
        for entry in (encoding, typecore):
            self.assertEqual((entry["status"], entry["reason"]), ("unavailable", "no-reviewed-profile"))
            self.assertNotIn("spec_sha256", entry)
        exports = {item["symbol"]: item for item in encoding["projection"]["exports"]}
        reader = exports["mulib_encoding_read_uvarint"]
        self.assertTrue(reader["definition_lines"])
        self.assertIn(("unsupported-pointer", "parameter[0]"),
                      {(issue["code"], issue.get("position")) for issue in reader["reasons"]})
        exports = {item["symbol"]: item for item in typecore["projection"]["exports"]}
        self.assertIn(("unsupported-record", "parameter[0]"),
                      {(issue["code"], issue.get("position"))
                       for issue in exports["mulib_typecore_read16be"]["reasons"]})

    def test_failed_projection_is_distinct_from_no_implementation(self):
        report, status = GEN["batch"](lambda: self.compiler, CONFIG, INCLUDE, REVIEW,
                                      project_headers=True, selected=["texdim", "encoding"])
        encoding, texdim = report["kernels"]
        self.assertEqual(status, 1)
        self.assertEqual(report["failed_projection"], 1)
        self.assertEqual(encoding["projection"]["status"], "projected")
        self.assertEqual(texdim["projection"]["status"], "failed")
        self.assertEqual([issue["code"] for issue in texdim["projection"]["errors"]],
                         ["c11-ast", "resolved-interface"])
        self.assertNotIn("exports", texdim["projection"])

    def test_supported_shape_is_not_review_and_missing_body_is_not_a_parse_error(self):
        directory = self.make_scratch()
        (directory / "mulib-test-projection.h").write_text("""
#include <stdint.h>
int64_t known_shape(int64_t x);
int64_t not_implemented(int64_t x);
int64_t unsupported_body(int64_t x);
int64_t known_shape(int64_t x) { return x; }
int64_t unsupported_body(int64_t x) { return x * x; }
""", encoding="ascii")
        report, status = GEN["batch"](lambda: self.compiler, CONFIG, directory, {}, project_headers=True)
        self.assertEqual(status, 0)
        entry, = report["kernels"]
        self.assertEqual(entry["status"], "unavailable")
        self.assertEqual(entry["projection"]["status"], "projected")
        exports = {item["symbol"]: item for item in entry["projection"]["exports"]}
        self.assertEqual(exports["known_shape"]["eligibility"], "shape-compatible")
        self.assertEqual(exports["not_implemented"]["reasons"][0]["code"], "no-implementation")
        self.assertEqual(exports["unsupported_body"]["reasons"][0]["code"], "unsupported-body")


if __name__ == "__main__":
    unittest.main()
