"""C11Interface analysis-model 1 decoder, with optional real CLI route checks."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import cephyr_interface as ci
from ffi_types import NativeGraph


FIXTURES = HERE / "fixtures"
SOURCE = (FIXTURES / "cephyr-interface-analysis.c").read_bytes()
WIRE = (FIXTURES / "cephyr-interface-analysis.sexp").read_bytes()
LEGACY = (FIXTURES / "cephyr-interface-v1.sexp").read_bytes()
TARGETS = ("x86_64-linux-gnu", "aarch64-linux-gnu", "riscv64-linux-gnu")
CEPHYR = LEGACY_CEPHYR = None
MEMORY_MODEL = "closed-unsigned-expression-v2"


def named(decoded):
    return {decl.name: decl for decl in decoded.declarations}


def mutate(old: str, new: str, *, subject: str = "", data: bytes = WIRE) -> bytes:
    """Replace one substring of a serializer line identified by name or form."""
    lines = data.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if subject.encode() in line and old.encode() in line:
            lines[i] = line.replace(old.encode(), new.encode(), 1)
            return b"".join(lines)
    raise AssertionError(f"not in fixture: {subject!r}, {old!r}")


def cli(executable, target, operation, lang="c", source=SOURCE, *args):
    result = subprocess.run([executable, f"--target={target}", *args, operation, "-x", lang, "-"],
                            input=source, capture_output=True, timeout=30)
    if result.returncode:
        raise AssertionError(result.stderr.decode("utf-8", errors="replace"))
    return result.stdout


class AnalysisTests(unittest.TestCase):
    def reject(self, data, code="schema", message=None):
        with self.assertRaises(ci.InterfaceError) as raised:
            ci.decode_interface(data)
        self.assertEqual(raised.exception.code, code, str(raised.exception))
        self.assertLess(len(str(raised.exception)), 150)
        if message is not None:
            self.assertIn(message, str(raised.exception))

    def test_legacy_has_no_invented_analysis_facts(self):
        decoded = ci.decode_interface(LEGACY)
        self.assertIsNone(decoded.analysis_model)
        self.assertIsNone(decoded.target_properties)
        self.assertTrue(all(not decl.declaration_sites for decl in decoded.declarations))
        self.assertTrue(all(tag.traits is None for tag in decoded.tags))
        fn = decoded.functions[0]
        self.assertIsNone(fn.call_contract)
        self.assertIsNone(fn.behavior)
        self.assertIsNone(fn.memory_safety)
        self.assertIsNone(fn.body_proof)
        self.assertIsNone(fn.portable_analysis_data())

    def test_decodes_real_typed_facts(self):
        decoded = ci.decode_interface(WIRE, expected_target=TARGETS[0])
        self.assertEqual(decoded.analysis_model, ci.AnalysisModel(
            1, ("declarations", "bounded-return-proof"), "c11-frontend", "not-a-guarantee"))
        self.assertEqual(decoded.target_properties, ci.TargetProperties(
            8, "little", 64, "twos-complement", True))
        declarations = named(decoded)
        tags = {tag.name: tag for tag in decoded.tags}
        self.assertEqual(declarations["Word"].type.value_range, ci.ValueRange(0, (1 << 64) - 1))
        self.assertEqual(declarations["signed_wide"].type.value_range,
                         ci.ValueRange(-(1 << 63), (1 << 63) - 1))
        self.assertEqual(declarations["plain_char"].type.value_range, ci.ValueRange(-128, 127))
        self.assertEqual(declarations["callback"].type.category, "function")
        self.assertEqual(declarations["opaque"].type.category, "object")
        self.assertEqual(tags["Child"].fields[1].bit_field.value_range, ci.ValueRange(-4, 3))
        self.assertIsNone(tags["Child"].fields[2].bit_field.value_range)
        self.assertEqual(tags["Child"].fields[3].bit_field.value_range, ci.ValueRange(0, 1))
        self.assertEqual(tags["Parent"].traits, ci.RecordTraits(True, True, True, False, True))
        self.assertEqual(tags["TailWrapper"].traits, ci.RecordTraits(True, False, False, True, False))
        self.assertIsNone(tags["E"].traits)
        self.assertIsNone(tags["Opaque"].traits)
        self.assertEqual(declarations["Word"].declaration_sites, (
            ci.DeclarationSite("presumed.h", 70, False, ()),
            ci.DeclarationSite("presumed.h", 71, False, ()),
        ))
        wide = declarations["wide"]
        self.assertEqual(wide.storage_duration, "static")
        self.assertEqual(tuple(site.definition for site in wide.declaration_sites), (False, True))

    def test_call_contracts_and_site_subjects(self):
        declarations = named(ci.decode_interface(WIRE))
        self.assertEqual(declarations["stop"].call_contract,
                         ci.CallContract("forbidden-by-declaration", 0, False))
        self.assertEqual(declarations["old_style"].call_contract, ci.CallContract("unknown", None, None))
        self.assertEqual(declarations["variadic"].call_contract, ci.CallContract("unknown", 1, True))
        self.assertEqual(declarations["variadic"].declaration_sites[0].parameters, (
            ci.SiteParameter(0, None, "register", False, None, ()),
        ))
        first, second = declarations["span"].declaration_sites
        self.assertEqual(first.parameters, (ci.SiteParameter(0, "array", None, False,
                                                             ci.ArrayAccess(4), ("restrict",)),))
        self.assertEqual(second.parameters, ())
        self.assertEqual(declarations["dynamic"].declaration_sites[0].parameters, (
            ci.SiteParameter(1, "array", "register", False, ci.ArrayAccess(None), ()),
        ))
        self.assertEqual(declarations["variable"].declaration_sites[0].parameters, (
            ci.SiteParameter(0, "array", None, True, None, ()),
        ))
        first, second = declarations["upgrade"].declaration_sites
        self.assertEqual((first.parameters, second.parameters[0].index), ((), 0))
        # Filename labels are not interpreted as paths, or even required to be nonempty.
        for spelling, file, line in [('""', "", 90), ('"/does/not/exist"', "/does/not/exist", 90),
                                     ('unknown', None, None)]:
            raw = mutate('(file "presumed.h") (line 90)', f'(file {spelling}) (line {line or "unknown"})',
                         subject='function "old_style"')
            [site] = named(ci.decode_interface(raw))["old_style"].declaration_sites
            self.assertEqual((site.file, site.line), (file, line))

    def test_bounded_results_keep_narrow_subjects_and_unknowns(self):
        declarations = named(ci.decode_interface(WIRE))
        fn = declarations["mask"]
        self.assertEqual(fn.declaration_sites[0],
                         ci.DeclarationSite("not-an-open-file.c", 120, True, ()))
        self.assertEqual(fn.memory_safety, ci.MemorySafety("proved", MEMORY_MODEL, "this-definition", 0, "satie"))
        self.assertEqual(fn.body_proof, ci.BodyProof(
            MEMORY_MODEL, "this-definition", "none-by-construction",
            (ci.ReturnDependency(0, "may-influence"), ci.ReturnDependency(1, "independent")),
            tuple(ci.ReturnPolicy(width, "proved") for width in (8, 16, 31)), "satie",
        ))
        self.assertEqual(fn.behavior.general_body_analysis, "not-performed")
        self.assertEqual(fn.behavior.memory_write, "unknown")
        self.assertEqual(fn.behavior.taint_transfer, "unknown")
        self.assertEqual(declarations["lookup"].memory_safety.checked_accesses, 1)
        unsafe = declarations["unsafe_lookup"]
        self.assertEqual(unsafe.memory_safety.status, "disproved")
        self.assertIsNone(unsafe.body_proof)
        for name in ("external_call", "opaque_body", "volatile_body"):
            self.assertEqual(declarations[name].memory_safety, ci.MemorySafety("unknown"))
            self.assertIsNone(declarations[name].body_proof)
        self.assertEqual(declarations["width_test"].body_proof.return_policies, (
            ci.ReturnPolicy(8, "disproved"), ci.ReturnPolicy(16, "proved"), ci.ReturnPolicy(31, "proved"),
        ))
        self.assertEqual(declarations["local"].body_proof.return_dependencies, ())
        self.assertEqual(declarations["inline_only"].definition, "inline-only")
        self.assertIsNotNone(declarations["inline_only"].body_proof)

        # A solver limit can leave an individual result or the whole proof unresolved.
        raw = mutate("(parameter 1 independent)", "(parameter 1 unknown)", subject='function "mask"')
        raw = mutate("(unsigned-width 8 proved)", "(unsigned-width 8 unknown)",
                     subject='function "mask"', data=raw)
        raw = mutate("(status disproved)", "(status unknown)", subject='function "unsafe_lookup"', data=raw)
        modified = named(ci.decode_interface(raw))
        self.assertEqual(modified["mask"].body_proof.return_dependencies[1].status, "unknown")
        self.assertEqual(modified["mask"].body_proof.return_policies[0].status, "unknown")
        self.assertEqual(modified["unsafe_lookup"].memory_safety.status, "unknown")
        self.assertIsNone(modified["unsafe_lookup"].body_proof)

    def test_portable_analysis_omits_presumed_locations(self):
        original = named(ci.decode_interface(WIRE))
        moved = named(ci.decode_interface(WIRE.replace(b'"presumed.h"', b'"/another/worktree/header.h"')
                                           .replace(b'(line 91)', b'(line 600)')
                                           .replace(b'"not-an-open-file.c"', b'"different-label"')))
        self.assertNotEqual(original["span"].declaration_sites, moved["span"].declaration_sites)
        self.assertEqual(original["span"].portable_analysis_data(), moved["span"].portable_analysis_data())
        span = original["span"].portable_analysis_data()
        self.assertEqual(span["declaration_sites"], [
            {"definition": False, "parameters": [{
                "index": 0, "adjusted_from": "array", "storage": None, "variable_bound": False,
                "requires_array_access": {"minimum_elements": 4}, "adjusted_pointer_qualifiers": ["restrict"],
            }]},
            {"definition": False, "parameters": []},
        ])
        mask = moved["mask"].portable_analysis_data()
        self.assertEqual(json.loads(json.dumps(mask)), mask)
        self.assertNotIn("different-label", json.dumps(mask))
        self.assertEqual(mask["body_proof"]["return_dependencies"][1], {"index": 1, "status": "independent"})
        self.assertEqual(mask["body_proof"]["return_policies"][0], {"width": 8, "status": "proved"})

    def test_rejects_unversioned_partial_or_wrong_target_extensions(self):
        for old, new, code in (
            ("analysis-model 1", "analysis-model 2", "version"),
            ("analysis-model 1", "analysis-model", "version"),
            ("analysis-model 1", 'analysis-model "1"', "version"),
            ("declarations bounded-return-proof", "declarations", "schema"),
            ("not-a-guarantee", "guaranteed", "schema"),
            ("(byte-order little)", "(byte-order big)", "target"),
            ("(byte-bits 8)", "(byte-bits 16)", "target"),
            ("(pointer-bits 64)", "(pointer-bits 32)", "target"),
            ("twos-complement", "ones-complement", "target"),
            ("(hosted true)", '(hosted "true")', "schema"),
            ("(hosted true)", "(hosted true) (hosted false)", "schema"),
        ):
            with self.subTest(old=old, new=new):
                self.reject(mutate(old, new), code)
        for clause in (b"analysis-model", b"target-properties"):
            self.reject(b"".join(line for line in WIRE.splitlines(keepends=True)
                                 if not line.strip().startswith(b"(" + clause + b" ")))
        # An additive clause without its version is not an extension-aware document.
        self.reject(LEGACY.replace(b'(data-model LP64)', b'(target-properties) (data-model LP64)'))
        self.reject(LEGACY.replace(b'(align 4)', b'(align 4) (value-range 0 1)', 1))

    def test_rejects_wrong_range_category_and_trait_subjects(self):
        for subject, old, new, code in (
            ('typedef "Word"', "(value-range 0 18446744073709551615)", "", "schema"),
            ('typedef "Word"', "18446744073709551615", "18446744073709551616", "limit"),
            ('typedef "Word"', "18446744073709551615", "18446744073709551614", "schema"),
            ('object "signed_wide"', "-9223372036854775808", "-9223372036854775807", "schema"),
            ('field "narrow"', "(value-range -4 3)", "(value-range -4 4)", "schema"),
            ("(width 0)", "(value-range unknown)", "(value-range 0 0)", "schema"),
            ('field "flag"', "(value-range 0 1)", "(value-range unknown)", "schema"),
            ('object "callback"', "(category function)", "(category object)", "schema"),
            ('object "opaque"', "(category object)", "(category function)", "schema"),
            ('struct "Parent"', "(contains-bit-field true)", "(contains-bit-field false)", "schema"),
            ('struct "Parent"', "(volatile-member true)", "(volatile-member false)", "schema"),
            ('union "TailWrapper"', "(contains-flexible-array true)", "(contains-flexible-array false)", "schema"),
            ('struct "Opaque"', "(complete false)", "(complete false) (traits)", "schema"),
            ('enum "E"', "(complete true)", "(complete true) (traits)", "schema"),
        ):
            with self.subTest(subject=subject, new=new):
                self.reject(mutate(old, new, subject=subject), code)

    def test_rejects_wrong_call_and_site_subjects(self):
        for subject, old, new in (
            ('object "wide"', "(storage-duration static)", "(storage-duration thread)"),
            ('object "wide"', "(storage extern none)", "(storage extern)"),
            ('object "wide"', "(definition true)", ""),
            ('object "signed_wide"', "(line 76)", "(line 76) (definition true)"),
            ('typedef "Word"', "(line 70)", "(line 70) (parameter 0 (storage register))"),
            ('typedef "Word"', "(line 70)", '(line "70")'),
            ('function "old_style"', "(file \"presumed.h\")", "(file unknown)"),
            ('function "old_style"', "(fixed-parameters unknown)", "(fixed-parameters 0)"),
            ('function "stop"', "(normal-return forbidden-by-declaration)", "(normal-return unknown)"),
            ('function "variadic"', "(variadic-tail true)", "(variadic-tail false)"),
            ('function "span"', "(fixed-parameters 1)", "(fixed-parameters 0)"),
            ('function "span"', "(parameter 0", "(parameter 1"),
            ('function "span"', "(adjusted-from array)", ""),
            ('function "span"', " 4) (adjusted-pointer", " 0) (adjusted-pointer"),
            ('function "span"', "qualifiers restrict", "qualifiers restrict restrict"),
            ('function "span"', "(line 92)", "(line 92) (definition false)"),
            ('function "dynamic"', "(parameter 1", "(parameter 0"),
            ('function "dynamic"', "(line 93)", "(line 93) (parameter 1 (storage register))"),
            ('function "variable"', "(variable-bound true)", "(variable-bound true) (requires-array-access 1)"),
            ('function "variable"', "(variable-bound true)", "(variable-bound false)"),
            ('function "variadic"', "(storage register)", "(storage static)"),
            ('function "stop"', "(general-body-analysis not-performed)", ""),
            ('function "stop"', "(taint-transfer unknown)", "(taint-transfer none)"),
        ):
            with self.subTest(subject=subject, new=new):
                self.reject(mutate(old, new, subject=subject))

    def test_rejects_malformed_and_inconsistent_bounded_proofs(self):
        for subject, old, new in (
            ('function "mask"', "(solver satie)", "(solver optional)"),
            ('function "mask"', "(scope this-definition)", "(scope translation-unit)"),
            ('function "mask"', "(status proved)", "(status unknown)"),
            ('function "mask"', "(checked-accesses 0)", "(checked-accesses 25)"),
            ('function "mask"', "(checked-accesses 0)", ""),
            ('function "mask"', "memory-effects none-by-construction", "memory-effects none"),
            ('function "mask"', "(parameter 1 independent)", "(parameter 0 independent)"),
            ('function "mask"', "(parameter 1 independent)", '(parameter "1" independent)'),
            ('function "mask"', "(parameter 1 independent)", "(parameter 1 untainted)"),
            ('function "mask"', "(parameter 1 independent)", ""),
            ('function "mask"', "(unsigned-width 8 proved)", "(unsigned-width 4 proved)"),
            ('function "mask"', "(unsigned-width 31 proved)", "(unsigned-width 16 proved)"),
            ('function "mask"', "(unsigned-width 31 proved)", "(unsigned-width 31 disproved)"),
            ('function "mask"', "(unsigned-width 31 proved)", ""),
            ('function "mask"', '(parameter-names "x" "secret")', '(parameter-names "x" "x")'),
            ('function "mask"', "(result (integer unsigned-int (rank 3) (bits 32) (signed false) "
                                "(size 4) (align 4) (value-range 0 4294967295)))", "(result void)"),
            ('function "unsafe_lookup"', "(status disproved)", "(status proved)"),
            ('function "unsafe_lookup"', "(checked-accesses 1)", "(checked-accesses 0)"),
            ('function "external_call"', "(status unknown)", "(status proved)"),
        ):
            with self.subTest(subject=subject, new=new):
                self.reject(mutate(old, new, subject=subject))
        raw = mutate("(definition defined)", "(definition declaration)", subject='function "mask"')
        raw = mutate("(definition true)", "", subject='function "mask"', data=raw)
        self.reject(raw, message="invalid function subject")
        raw = mutate("(specifiers)", "(specifiers noreturn)", subject='function "mask"')
        raw = mutate("(normal-return unknown)", "(normal-return forbidden-by-declaration)",
                     subject='function "mask"', data=raw)
        self.reject(raw, message="invalid function subject")

    def test_refuses_bound_excess_without_downgrading(self):
        with patch.object(ci, "MAX_REDECLARATIONS", 1):
            self.reject(WIRE, "limit", "declaration site count")
        with patch.object(ci, "_PROOF_DEFINITION_LIMIT", 1):
            self.reject(WIRE, message="invalid function subject")


class RealRouteTests(unittest.TestCase):
    def test_analysis_routes_and_all_reviewed_targets(self):
        if CEPHYR is None:
            self.skipTest("pass --cephyr to test the real analysis CLI")
        for target in TARGETS:
            with self.subTest(target=target):
                raw = cli(CEPHYR, target, "--emit-interface=sexp")
                direct = ci.decode_interface(raw, expected_target=target)
                self.assertEqual(direct.analysis_model.version, 1)
                self.assertTrue(direct.target_properties.hosted)
                if target == TARGETS[0]:
                    self.assertEqual(raw, WIRE)
                expected_char = ci.ValueRange(-128, 127) if target == TARGETS[0] else ci.ValueRange(0, 255)
                self.assertEqual(named(direct)["plain_char"].type.value_range, expected_char)
                self.assertEqual(named(direct)["lookup"].memory_safety.status, "proved")
                for operation, language in (("-E", "cpp-output"), ("--emit-ast=json", "c11-ast-json"),
                                            ("--emit-ast=sexp", "c11-ast-sexp")):
                    intermediate = cli(CEPHYR, target, operation)
                    projection = cli(CEPHYR, target, "--emit-interface=sexp", language, intermediate)
                    self.assertEqual(ci.decode_interface(projection, expected_target=target), direct)
                freestanding = cli(CEPHYR, target, "--emit-interface=sexp", "c", SOURCE, "-ffreestanding")
                self.assertFalse(ci.decode_interface(freestanding).target_properties.hosted)

    def test_real_include_has_only_presumed_paths_and_portable_analysis(self):
        if CEPHYR is None:
            self.skipTest("pass --cephyr to test the real analysis CLI")
        include = HERE.parents[3] / "include"
        for target in TARGETS:
            with self.subTest(target=target):
                raw = cli(CEPHYR, target, "--emit-interface=sexp", "c", b'#include "mulib-clamp.h"\n',
                          "-I", str(include))
                decl = named(ci.decode_interface(raw, expected_target=target))["mulib_clamp_i64"]
                [site] = decl.declaration_sites
                self.assertEqual(site.file, str(include / "mulib-clamp.h"))
                self.assertGreater(site.line, 0)
                self.assertNotIn(str(include), json.dumps(decl.portable_analysis_data()))
                self.assertEqual(decl.memory_safety.status, "unknown")
                self.assertIsNone(decl.body_proof)

    def test_legacy_cli_does_not_invent_facts_or_change_native_representation(self):
        if LEGACY_CEPHYR is None:
            self.skipTest("pass --legacy-cephyr to test the real e10 CLI")
        decoded = ci.decode_interface(cli(LEGACY_CEPHYR, TARGETS[0], "--emit-interface=sexp"))
        extended = ci.decode_interface(WIRE)
        self.assertIsNone(decoded.analysis_model)
        self.assertTrue(all(not decl.declaration_sites for decl in decoded.declarations))
        self.assertTrue(all(fn.memory_safety is None for fn in decoded.functions))
        left, right = NativeGraph(decoded.tags), NativeGraph(extended.tags)
        self.assertEqual(tuple(decl.name for decl in decoded.declarations),
                         tuple(decl.name for decl in extended.declarations))
        self.assertEqual([left.type(decl.type) for decl in decoded.declarations],
                         [right.type(decl.type) for decl in extended.declarations])
        self.assertEqual(left.tags(), right.tags())


if __name__ == "__main__":
    argv = argparse.ArgumentParser(add_help=False)
    argv.add_argument("--cephyr", type=Path)
    argv.add_argument("--legacy-cephyr", type=Path)
    args, remaining = argv.parse_known_args()
    CEPHYR = args.cephyr.resolve(strict=True) if args.cephyr else None
    LEGACY_CEPHYR = args.legacy_cephyr.resolve(strict=True) if args.legacy_cephyr else None
    unittest.main(argv=[sys.argv[0], *remaining])
