#!/usr/bin/env python3
"""Qualify paired C11Syntax/C11Interface evidence with real Cephyr and LLSynth."""
import argparse
from dataclasses import replace
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from cephyr_interface import InterfaceError, decode_interface
from cephyr_syntax import declaration_syntax, pair_with_interface
from ffgen import Override, generate_module
from ffi_types import target_identity
import ffic
from project_interface import CephyrProvider, ProjectionError


REVISION = "e10ac69eed7baf91b06d15c4e0d58360097cb5c4"
TARGET = target_identity("x86_64-linux-gnu")
SOURCE = b"""
typedef int Unary(int);
extern Unary mulib_x_typed;
extern int (*mulib_x_not_function)(int);
extern void mulib_x_halt(void);
_Noreturn void mulib_x_halt(void);
inline int mulib_x_inline_only(void) { return 7; }
/* A presumed source label must not be accepted as declaration ownership. */
#line 100 "include/mulib-x.h"
static inline int mulib_x_body(int value) {
    extern _Noreturn void mulib_x_block_local(void);
    return value;
}
int mulib_x_plain(void); /* _Noreturn */
"""
ENTRY = SimpleNamespace(key="x", module="mulib.x", filename="x.ffi",
                        header="mulib-x.h", header_path="include/mulib-x.h",
                        implementation="switch", reserved_prefix="mulib_x_")


class EvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        local = tempfile.TemporaryDirectory(dir=ARGS.scratch.resolve(strict=True))
        cls.addClassCleanup(local.cleanup)
        cls.work = Path(local.name)
        cls.parser = ffic.load_parser(ARGS.parser)
        cls.provider = CephyrProvider(
            ARGS.cephyr, TARGET[0], libstone_revision=REVISION, scratch_root=cls.work,
            tool_inputs=ARGS.tool_input, require_syntax=True,
        )
        cls.raw_ast = cls.provider.project_syntax(SOURCE).output
        projection = cls.provider.project(cls.raw_ast, language="c11-ast-sexp")
        cls.raw_interface = projection.output
        cls.interface = decode_interface(projection.output, expected_target=TARGET[0])
        cls.syntax = declaration_syntax(cls.raw_ast)
        cls.outcome = generate_module(ENTRY, cls.interface, TARGET, syntax=cls.syntax)

    def report(self, data):
        path = self.work / "x.ffi"
        path.write_bytes(data)
        return ffic.compile_files(self.parser, [path], target=TARGET[0], canonical_files=True)

    def test_resolved_and_structural_facts_but_no_origin_or_admission(self):
        self.assertEqual(self.raw_interface, self.provider.project(SOURCE).output)
        observed = pair_with_interface(self.syntax, self.interface)
        self.assertEqual(observed["mulib_x_halt"].declarations, 2)
        self.assertFalse(observed["mulib_x_halt"].has_body)
        self.assertNotIn("mulib_x_block_local", observed)
        self.assertNotIn("mulib_x_not_function", observed)
        self.assertIn("mulib_x_typed", observed)
        report = self.report(self.outcome.document)
        self.assertEqual(report["bindings"], [])
        rows = {row["member"]: row for row in report["discovery"]}
        for member, facts in (
            ("body", ["syntax_body"]), ("halt", ["declared_noreturn"]),
            ("inline_only", ["inline_only", "syntax_body"]),
        ):
            self.assertEqual(rows[member]["reason"], "ambiguous_export")
            self.assertEqual(rows[member]["c11_evidence"],
                             {"source": "cephyr_c11_v1", "facts": facts})
        self.assertNotIn("c11_evidence", rows["plain"])
        self.assertNotIn("c11_evidence", rows["typed"])
        self.assertEqual(self.outcome.document.count(b"cephyr_c11_v1"), 3)
        self.provider.verify_inputs()

    def test_evidence_preserves_existing_explicit_selection_and_id(self):
        sym = "mulib_x_body"
        selection = Override(TARGET, ENTRY.module, (sym,))
        paired = generate_module(ENTRY, self.interface, TARGET, syntax=self.syntax, override=selection)
        scalar_only = generate_module(ENTRY, self.interface, TARGET, override=selection)
        bound, = self.report(paired.document)["bindings"]
        baseline, = self.report(scalar_only.document)["bindings"]
        self.assertEqual(bound["symbol"], sym)
        self.assertEqual(bound["id"], baseline["id"])
        self.assertEqual(bound["native_fingerprint"], baseline["native_fingerprint"])
        self.assertEqual(bound["c11_evidence"]["facts"], ["syntax_body"])
        for marker, diagnostic in (
            (b"declared_noreturn syntax_body", "cannot describe a callable scalar"),
            (b"inline_only syntax_body", "cannot describe a callable scalar"),
        ):
            with self.assertRaisesRegex(ffic.Invalid, diagnostic):
                self.report(paired.document.replace(b"v1 (syntax_body)", b"v1 (" + marker + b")"))

    def test_bad_or_unattached_directives_are_rejected(self):
        data = self.outcome.document
        for old, replacement, diagnostic in (
            (b"evidence c::mulib_x_body", b"evidence c::mulib_x_missing", "unknown or duplicate"),
            (b"(syntax_body)", b"(body_pure)", "unknown, duplicate or unordered"),
            (b"(syntax_body)", b"(syntax_body syntax_body)", "unknown, duplicate or unordered"),
            (b"(inline_only syntax_body)", b"(syntax_body inline_only)", "unknown, duplicate or unordered"),
            (b"(inline_only syntax_body)", b"(inline_only)", "requires a syntax body"),
        ):
            with self.subTest(replacement=replacement), self.assertRaisesRegex(ffic.Invalid, diagnostic):
                self.report(data.replace(old, replacement))
        directive = b"evidence c::mulib_x_body cephyr_c11_v1 (syntax_body);"
        with self.assertRaisesRegex(ffic.Invalid, "unknown or duplicate"):
            self.report(data.replace(directive, directive + b"\n" + directive))

    def test_cross_check_rejects_stale_or_forged_ast(self):
        with self.assertRaisesRegex(InterfaceError, "disagree"):
            pair_with_interface(tuple(replace(row, has_body=True)
                                      if row.name == "mulib_x_plain" else row for row in self.syntax),
                                self.interface)
        with self.assertRaisesRegex(InterfaceError, "version 1"):
            declaration_syntax(self.raw_ast.replace(b"(1 ", b"(2 ", 1))
        # Change the parser half only, not .source.text. Cephyr must reject the
        # serialized AST before any typed declaration evidence is produced.
        forged = self.raw_ast.replace(b'("name" "mulib_x_plain")', b'("name" "mulib_x_fake")')
        self.assertNotEqual(forged, self.raw_ast)
        with self.assertRaises(ProjectionError) as failed:
            self.provider.project(forged, language="c11-ast-sexp")
        self.assertEqual(failed.exception.code, "cephyr_failed")
        self.assertIn(b"cephyr.ast", failed.exception.diagnostics)

    def test_native_graph_can_corroborate_definition_and_specifier_facts(self):
        raw = b'''
ffi 3;
target "x86_64-linux-gnu" LP64 "SysVAMD64" sysv;
foreign mulib.x {
    provider mulib "x";
    header "mulib-x.h";
    graph 0 {}
    discover halt = c::mulib_x_halt native external declaration graph 0
        function-type(fixed,(),void) specifiers(noreturn) reason noreturn;
    evidence c::mulib_x_halt cephyr_c11_v1 (declared_noreturn);
}'''
        report = self.report(raw)
        self.assertEqual(report["bindings"], [])
        self.assertEqual(report["discovery"][0]["c11_evidence"]["facts"], ["declared_noreturn"])
        for body, diagnostic in (
            (raw.replace(b"specifiers(noreturn) ", b""), "contradicts native specifiers"),
            (raw.replace(b"(declared_noreturn)", b"(declared_noreturn syntax_body)"),
             "contradicts native definition"),
        ):
            with self.assertRaisesRegex(ffic.Invalid, diagnostic):
                self.report(body)

    def test_relocated_diagnostic_labels_do_not_change_normalized_evidence(self):
        def project_at(name):
            root = self.work / name
            root.mkdir()
            (root / "test.h").write_bytes(SOURCE)
            provider = CephyrProvider(
                ARGS.cephyr, TARGET[0], libstone_revision=REVISION, scratch_root=self.work,
                tool_inputs=ARGS.tool_input, include_dirs=(root,), require_syntax=True,
            )
            syntax = provider.project_syntax(b'#include <test.h>\n').output
            projection = provider.project(syntax, language="c11-ast-sexp").output
            provider.verify_inputs()
            return syntax, declaration_syntax(syntax), projection

        a, b = project_at("left"), project_at("right")
        self.assertNotEqual(a[0], b[0])
        self.assertEqual(a[1:], b[1:])

    def test_body_present_is_not_a_body_semantics_claim(self):
        broken = b"static inline int mulib_x_broken(void) { return undeclared; }\n"
        ast = self.provider.project_syntax(broken)
        projection = self.provider.project(ast.output, language="c11-ast-sexp")
        interface = decode_interface(projection.output, expected_target=TARGET[0])
        out = generate_module(ENTRY, interface, TARGET, syntax=declaration_syntax(ast.output))
        self.assertIn(b"cephyr_c11_v1 (syntax_body)", out.document)
        self.assertEqual(self.report(out.document)["bindings"], [])
        lowered = subprocess.run([ARGS.cephyr, "--target=" + TARGET[0], "--stdlib=none",
                                  "-nostdinc", "-S", "-x", "c11-ast-sexp", "-"],
                                 input=ast.output, capture_output=True, cwd=self.work, timeout=15)
        self.assertNotEqual(lowered.returncode, 0)
        self.assertIn(b"undeclared", lowered.stderr)

    def test_catalogue_and_verified_dsl_evidence_must_agree(self):
        report = self.report(self.outcome.document)
        evidence = {(row["module"], row["symbol"]): row["c11_evidence"]
                    for row in report["discovery"] if "c11_evidence" in row}
        inputs = {"x.ffi": ffic.sha(self.outcome.document)}
        index = dict(schema="on1x.stdffi.mulib/v1", target=TARGET[0],
                     modules=[dict(key="x", module="mulib.x", file="x.ffi",
                                   ffi_sha256=inputs["x.ffi"], symbols=self.outcome.metadata["symbols"])])
        path = self.work / "index.json"
        path.write_text(json.dumps(index), encoding="utf-8")
        ffic.verify_catalogue(path, inputs, TARGET[0], evidence)
        with self.assertRaisesRegex(ffic.Invalid, "C11 evidence directives differ"):
            ffic.verify_catalogue(path, inputs, TARGET[0], {})


if __name__ == "__main__":
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--cephyr", required=True, type=Path)
    cli.add_argument("--parser", required=True, type=Path)
    cli.add_argument("--scratch", required=True, type=Path)
    cli.add_argument("--tool-input", action="append", required=True, type=Path)
    ARGS = cli.parse_args()
    unittest.main(argv=[sys.argv[0]])
