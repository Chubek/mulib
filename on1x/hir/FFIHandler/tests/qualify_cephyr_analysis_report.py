#!/usr/bin/env python3
"""Round-trip real Cephyr analysis-model 1 through ffgen, LLSynth, and ffic."""
import argparse
from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from cephyr_interface import decode_interface
from cephyr_syntax import declaration_syntax
from ffgen import ANALYSIS_REVISION, Override, generate_module, projection_digest
from ffi_types import target_identity
import ffic
from project_interface import CephyrProvider


SOURCE = b"""
#line 42 "include/mulib-x.h"
unsigned mulib_x_mask(unsigned x) { return x & 255u; }
unsigned mulib_x_lookup(unsigned x) {
    unsigned table[2] = {1u, 2u};
    return x < 2u ? table[x] : 0u;
}
unsigned mulib_x_bad_index(unsigned x) {
    unsigned table[2] = {1u, 2u};
    return table[x];
}
void mulib_x_access(unsigned n, int values[static 2+3]);
void mulib_x_access(unsigned n, int values[static n]);
void mulib_x_access(unsigned n, int values[static 7]);
void mulib_x_legacy();
void mulib_x_varargs(int n, ...);
_Noreturn void mulib_x_stop(void);
extern unsigned foreign(unsigned x);
unsigned mulib_x_opaque(unsigned x) { return foreign(x); }
"""
ENTRY = SimpleNamespace(key="x", module="mulib.x", filename="x.ffi", header="mulib-x.h",
                        header_path="include/mulib-x.h", implementation="switch",
                        reserved_prefix="mulib_x_")
TARGET = target_identity("x86_64-linux-gnu")


class AnalysisTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(dir=OPTIONS.scratch)
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.parser = ffic.load_parser(OPTIONS.parser)
        cls.interface, cls.syntax, _ = cls.project(SOURCE)
        cls.generation = generate_module(ENTRY, cls.interface, TARGET, syntax=cls.syntax)

    @classmethod
    def project(cls, source, target=TARGET[0], **options):
        provider = CephyrProvider(
            OPTIONS.cephyr, target, libstone_revision=ANALYSIS_REVISION,
            scratch_root=cls.root, tool_inputs=OPTIONS.tool_input, require_syntax=True, **options,
        )
        ast = provider.project_syntax(source).output
        raw = provider.project(ast, language="c11-ast-sexp").output
        provider.verify_inputs()
        return decode_interface(raw, expected_target=target), declaration_syntax(ast), raw

    def compile(self, document):
        path = self.root / ENTRY.filename
        path.write_bytes(document)
        return ffic.compile_files(self.parser, [path], TARGET[0], canonical_files=True)

    def test_typed_facts_are_not_admission_or_general_effects(self):
        report = self.compile(self.generation.document)
        self.assertEqual(report["bindings"], [])
        self.assertNotIn(b"include/mulib-x.h", self.generation.document)
        self.assertTrue(all(row["reason"] == "ambiguous_export" for row in report["discovery"]))
        analysis = report["modules"][0]["c11_analysis"]
        self.assertEqual(analysis, self.generation.metadata["c11_analysis"])
        self.assertEqual(analysis["target_properties"], dict(
            byte_bits=8, byte_order="little", pointer_bits=64,
            integer_representation="twos-complement", hosted=True))
        functions = {row["symbol"].removeprefix("mulib_x_"): row for row in analysis["functions"]}
        self.assertEqual(functions["access"]["requires_array_access"], [
            dict(declaration=0, parameter=1, minimum_elements=5),
            dict(declaration=2, parameter=1, minimum_elements=7),
        ])
        self.assertEqual(functions["legacy"]["call_contract"], dict(
            fixed_parameters=None, variadic_tail=None, normal_return="unknown"))
        self.assertEqual(functions["varargs"]["call_contract"], dict(
            fixed_parameters=1, variadic_tail=True, normal_return="unknown"))
        self.assertEqual(functions["stop"]["call_contract"]["normal_return"], "forbidden-by-declaration")
        mask = functions["mask"]["body_proof"]
        self.assertEqual(mask["scope"], "this-definition")
        self.assertEqual(mask["return_policies"], [[8, "proved"], [16, "proved"], [31, "proved"]])
        self.assertEqual(mask["return_dependencies"], ["may-influence"])
        self.assertEqual(functions["lookup"]["memory_safety"]["checked_accesses"], 1)
        self.assertEqual(functions["lookup"]["memory_safety"]["status"], "proved")
        self.assertEqual(functions["bad_index"]["memory_safety"]["status"], "disproved")
        self.assertNotIn("body_proof", functions["bad_index"])
        self.assertEqual(functions["opaque"]["memory_safety"], {"status": "unknown"})
        self.assertNotIn("body_proof", functions["opaque"])

    def test_analysis_does_not_change_scalar_descriptor(self):
        selection = Override(TARGET, ENTRY.module, ("mulib_x_mask",))
        with_analysis = generate_module(ENTRY, self.interface, TARGET, syntax=self.syntax,
                                        override=selection).document
        old_document = with_analysis.split(b"    analysis ", 1)[0] + b"}\n"
        current, legacy = (self.compile(raw) for raw in (with_analysis, old_document))
        self.assertEqual(current["bindings"], legacy["bindings"])
        self.assertEqual(current["bindings"][0]["effects"], "unknown")
        self.assertNotIn("c11_analysis", legacy["modules"][0])

    def test_reject_malformed_or_contradictory_reports(self):
        mutations = (
            (b"(8 little 64", b"(8 big 64", "invalid C11 analysis value"),
            (b"analyze c::mulib_x_access", b"analyze c::mulib_x_other", "analysis symbol"),
            (b"requires-array-access (0 1 5)", b"requires-array-access (0 2 5)",
             "array-access obligation"),
            (b"requires-array-access (0 1 5)", b"requires-array-access (0 1 0)",
             "out-of-bounds C11 analysis number"),
            (b"requires-array-access (2 1 7)", b"requires-array-access (0 1 7)",
             "array-access obligation"),
            (b"memory-safety (proved closed-unsigned", b"memory-safety (unknown closed-unsigned",
             "requires proved modeled memory"),
            (b"this-definition none-by-construction", b"all-definitions none-by-construction",
             "invalid C11 body proof model"),
            (b"none-by-construction satie", b"globally-pure satie", "invalid C11 body proof model"),
            (b"16 proved 31", b"8 proved 31", "invalid C11 return policies"),
            (b"return-dependencies (may-influence)", b"return-dependencies (untainted)",
             "invalid C11 return dependencies"),
            (b"0 false forbidden-by-declaration", b"0 false unknown", "contradicts declaration evidence"),
        )
        raw = self.generation.document
        for before, after, diagnostic in mutations:
            with self.subTest(after=after), self.assertRaisesRegex(ffic.Invalid, diagnostic):
                self.assertIn(before, raw)
                self.compile(raw.replace(before, after, 1))
        line = next(line for line in raw.splitlines(keepends=True) if b"analyze c::mulib_x_mask" in line)
        with self.assertRaisesRegex(ffic.Invalid, "does not cover emitted symbols"):
            self.compile(raw.replace(line, b""))

    def test_exact_index_report_in_publish_and_registry(self):
        document = self.generation.document
        report = self.compile(document)
        facts = {(row["module"], row["symbol"]): row["c11_evidence"]
                 for row in report["discovery"] if "c11_evidence" in row}
        analysis = {ENTRY.module: report["modules"][0]["c11_analysis"]}
        inputs = {ENTRY.filename: ffic.sha(document)}
        index = dict(schema="on1x.stdffi.mulib/v1", target=TARGET[0], modules=[dict(
            key=ENTRY.key, module=ENTRY.module, file=ENTRY.filename, ffi_sha256=inputs[ENTRY.filename],
            **self.generation.metadata)])
        path = self.root / "index.json"
        path.write_text(json.dumps(index), encoding="utf-8")
        shard = self.root / "unit_0.json"
        self.assertEqual(ffic.main([
            "--parser", str(OPTIONS.parser), "--unit", "unit_0",
            "--target", TARGET[0], "--canonical-files", "--header", str(shard.with_suffix(".hpp")),
            "--manifest", str(shard), str(self.root / ENTRY.filename),
        ]), 0)

        def verify(valid):
            if valid:
                ffic.verify_catalogue(path, inputs, TARGET[0], facts, analysis)
                _, registry = ffic.registry_index([shard], TARGET[0], path)
                self.assertIs(registry["grants_safe_admission"], False)
            else:
                with self.assertRaisesRegex(ffic.Invalid, "C11 analysis directives differ"):
                    ffic.verify_catalogue(path, inputs, TARGET[0], facts, analysis)
                with self.assertRaisesRegex(ffic.Invalid, "C11 analysis directives differ"):
                    ffic.registry_index([shard], TARGET[0], path)

        verify(True)
        for mutate in (
                lambda row: row.pop("c11_analysis"),
                lambda row: row["c11_analysis"]["target_properties"].update(hosted=1),
                lambda row: row["c11_analysis"]["functions"][0]["requires_array_access"][0].update(
                    minimum_elements=6)):
            altered = deepcopy(index)
            mutate(altered["modules"][0])
            path.write_text(json.dumps(altered), encoding="utf-8")
            verify(False)

    def test_freestanding_exact_target_and_presumed_labels(self):
        source = b'unsigned mulib_x_mask(unsigned x) { return x & 255u; }\n'
        for triple in ("aarch64-linux-gnu", "riscv64-linux-gnu"):
            with self.subTest(triple=triple):
                interface, syntax, _ = self.project(source, triple, freestanding=True)
                generated = generate_module(ENTRY, interface, target_identity(triple), syntax=syntax)
                mod, = ffic.parse_document(self.parser, generated.document, ENTRY.filename)
                self.assertIs(mod["c11_analysis"]["target_properties"]["hosted"], False)
        captures = []
        for location in ("left", "right"):
            root = self.root / location
            root.mkdir()
            (root / "test.h").write_bytes(source)
            interface, syntax, raw = self.project(b'#include <test.h>\n', include_dirs=(root,))
            generated = generate_module(ENTRY, interface, TARGET, syntax=syntax)
            captures.append((raw, projection_digest(interface, raw), generated))
        self.assertNotEqual(captures[0][0], captures[1][0])
        self.assertEqual(captures[0][1:], captures[1][1:])


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--cephyr", required=True, type=Path)
    cli.add_argument("--parser", required=True, type=Path)
    cli.add_argument("--scratch", required=True, type=Path)
    cli.add_argument("--tool-input", action="append", required=True, type=Path)
    OPTIONS = cli.parse_args()
    unittest.main(argv=[sys.argv[0]])
