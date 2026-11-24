"""C11Interface v1 reader unit and optional explicit W02 artifact tests.

Run directly with Python 3; pass --w02-fixtures DIR to also require the real
Cephyr artifacts specified by W02. The checked-in .sexp is a schema fixture,
not an output from Cephyr.
"""
from __future__ import annotations

import argparse
from dataclasses import FrozenInstanceError
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tools"))
import cephyr_interface as ci


TARGETS = (
    ("x86_64-linux-gnu", "SysVAMD64", "sysv", True, "x87", 80),
    ("aarch64-linux-gnu", "AAPCS64", "aapcs64", False, "binary128", 128),
    ("riscv64-linux-gnu", "LP64", "lp64", False, "binary128", 128),
)
AMD64 = TARGETS[0][0]
INT = "(integer int (rank 3) (bits 32) (signed true) (size 4) (align 4))"
UINT = "(integer unsigned-int (rank 3) (bits 32) (signed false) (size 4) (align 4))"
CHAR = "(integer char (rank 1) (bits 8) (signed true) (size 1) (align 1))"
W02_FIXTURES = None


def document(*forms: str, target=AMD64, abi="SysVAMD64", model="LP64", version="1") -> bytes:
    return (
        f'(interface {version}\n (target "{target}") (data-model {model}) (abi "{abi}")\n'
        + "\n".join(forms) + ")\n"
    ).encode()


def typedef(type_: str, name='"T"') -> str:
    return f"(typedef {name} (type {type_}))"


def field(type_: str, name='"m"', offset=0, extra="") -> str:
    return f"(field {name} (offset {offset}) {extra} (type {type_}))"


def tag(tag_id: int, *fields: str, size=4, align=4, kind="struct", name='"S"') -> str:
    return (f"(tag {tag_id} {kind} {name} (complete true) (layout (size {size}) (align {align})) "
            + " ".join(fields) + ")")


def pointer(pointee: str) -> str:
    return f"(pointer (bits 64) (size 8) (align 8) (pointee {pointee}))"


def array(element: str, count="unknown", size="unknown", align=4) -> str:
    return f"(array (count {count}) (size {size}) (align {align}) (element {element}))"


def function_type(result="void", params="", prototype="true", variadic="false") -> str:
    return (f"(function-type (prototype {prototype}) (variadic {variadic}) "
            f"(result {result}) (parameters {params}))")


def function(type_: str, names="", cc="sysv", state="declaration", specs="", name='"f"') -> str:
    return (f"(function {name} (linkage external) (storage none) (definition {state}) "
            f"(calling-convention {cc}) (parameter-names {names}) (specifiers {specs}) (type {type_}))")


def object_(type_: str, size="4", align="4", definition="declaration") -> str:
    return (f'(object "o" (linkage external) (storage extern) (definition {definition}) '
            f'(layout (size {size}) (align {align})) (type {type_}))')


class ReaderTests(unittest.TestCase):
    def reject(self, data: bytes, code="schema", message=None, **kwargs):
        with self.assertRaises(ci.InterfaceError) as failure:
            ci.decode_interface(data, **kwargs)
        self.assertEqual(failure.exception.code, code, str(failure.exception))
        if message is not None:
            self.assertIn(message, str(failure.exception))
        self.assertLess(len(str(failure.exception)), 150)

    def test_full_typed_fixture(self):
        decoded = ci.decode_interface((HERE / "fixtures/cephyr-interface-v1.sexp").read_bytes(),
                                      expected_target=AMD64)
        self.assertEqual((decoded.version, decoded.target, decoded.abi, decoded.data_model),
                         (1, AMD64, "SysVAMD64", "LP64"))
        node, incomplete, enumeration = decoded.tags
        self.assertEqual((node.id, incomplete.id, node.name, incomplete.name), (0, 1, "Node", "Node"))
        self.assertEqual(node.layout, ci.Layout(16, 8))
        self.assertIsNone(incomplete.layout)
        self.assertIsNone(enumeration.name)
        self.assertEqual(tuple(e.value for e in enumeration.enumerators), (-2, 0))
        next_field, bit_field, tail = node.fields
        self.assertEqual(next_field.type, ci.PointerType(64, 8, 8, ci.TagRef(0)))
        self.assertEqual((bit_field.offset, bit_field.bit_field), (8, ci.BitField(3, 0, 8)))
        self.assertTrue(tail.flexible_array)
        self.assertEqual((tail.type.count, tail.type.size, tail.type.align), (None, None, 1))
        opaque, word, wide, obj, flag, visit = decoded.declarations
        self.assertEqual(opaque, ci.Typedef("Opaque", ci.TagRef(1)))
        self.assertEqual((word.type.size, wide.type.size), (8, 8))
        self.assertNotEqual(word.type, wide.type)
        self.assertEqual(obj.layout, ci.Layout(None, None))
        self.assertEqual(flag.layout, ci.Layout(1, 16))
        self.assertEqual(flag.type, ci.IntegerType("bool", 0, 1, False, 1, 1))
        self.assertEqual(flag.storage, ("static", "extern"))
        self.assertEqual(decoded.functions, (visit,))
        self.assertEqual(visit.parameter_names, (None, "none"))
        self.assertEqual(visit.type.result, ci.TagRef(2))
        pointee = visit.type.parameters[0].pointee
        self.assertEqual(pointee, ci.QualifiedType(("const", "volatile"), ci.TagRef(0)))
        self.assertTrue(visit.type.variadic)
        with self.assertRaises(FrozenInstanceError):
            decoded.version = 2

    def test_json_strings_are_not_atoms(self):
        escaped_name = r'"e\"sc\\ape\/\b\f\n\r\t\u00E9\uD83D\uDE00"'
        source = document(typedef(INT, escaped_name), '(tag 7 struct "none" (complete false))',
                          typedef(INT, '"unknown"'), typedef(INT, '"한국어"'))
        doc = ci.decode_interface(source)
        self.assertEqual(doc.tags[0].name, "none")
        self.assertEqual(doc.declarations[0].name, 'e"sc\\ape/\b\f\n\r\té😀')
        self.assertEqual(doc.declarations[1].name, "unknown")
        self.assertEqual(doc.declarations[2].name, "한국어")
        for source in (
            document(typedef(INT, "none")), document(typedef(INT, "unknown")),
            document(typedef(INT, "1")), document(typedef('("tag-ref" 0)')),
            document(typedef('("integer" int (rank 3) (bits 32) (signed true) (size 4) (align 4))')),
            document(typedef(INT, '""')), document(typedef(array(INT, '"unknown"'))),
            document(typedef(function_type(prototype='"true"'))),
            document(typedef(INT.replace("int (rank", '"int" (rank'))),
        ):
            with self.subTest(source=source):
                self.reject(source)

    def test_malformed_lexical_forms(self):
        for source in (
            b"", b"  ", b"interface", b")", b"(interface 1", b"\xef\xbb\xbf" + document(),
            document() + b" (interface 1)", document() + b")", document() + b"\x00",
            document(typedef(INT, br'"bad\x22"'.decode())), document(typedef(INT, '"bad\\q"')),
            document(typedef(INT, '"bad\\u1"')), document(typedef(INT, '"bad\\uDD12"')),
            document(typedef(INT, '"bad\\uD800\\u0000"')), document(typedef(INT, '"bad\n"')),
            document(typedef(INT, '"bad\x01"')), document(typedef(INT, '"bad"')).replace(b"bad", b"\xff"),
            b'(interface "unterminated', b"(interface 1true)", b'(interface 1 "a""b")',
            b"(interface 1 ;comment\n)", b"(interface 1 '#.(run))", b"(interface +1)",
            document() + b"\xc2\xa0",
        ):
            with self.subTest(source=source):
                self.reject(source, "syntax")

    def test_formatting_and_singleton_clause_order_do_not_matter(self):
        one = document(typedef(INT))
        two = b''' (interface\t1
        (abi "SysVAMD64")
        (typedef "T" (type (integer int(align 4)(bits 32)(size 4)(signed true)(rank 3))))
        (data-model LP64) (target "x86_64-linux-gnu") ) \r\n'''
        self.assertEqual(ci.decode_interface(one), ci.decode_interface(two))

    def test_version_and_envelope_identity(self):
        for version in ("2", "0", '"1"', "one"):
            self.reject(document(version=version), "version")
        self.reject(b"(1 (object whatever))")
        self.reject(b"(interface)", "version")
        self.reject(document(target="amd64"), "target")
        self.reject(document(abi="AAPCS64"), "target")
        self.reject(document(model="ILP32"), "target")
        self.reject(document(model='"LP64"'), "target")
        self.reject(document(), "target", expected_target="aarch64-linux-gnu")
        self.reject(document().replace(f'"{AMD64}"'.encode(), AMD64.encode()))

    def test_target_primitive_and_calling_convention_constraints(self):
        for target, abi, convention, signed, fmt, width in TARGETS:
            with self.subTest(target=target):
                char = CHAR if signed else CHAR.replace("signed true", "signed false")
                extended = f"(real long-double (format {fmt}) (bits {width}) (size 16) (align 16))"
                doc = ci.decode_interface(document(function(function_type(extended, char), "none", convention),
                                                   target=target, abi=abi), expected_target=target)
                self.assertEqual(doc.functions[0].type.result, ci.RealType("long-double", fmt, width, 16, 16))
                self.assertEqual(doc.functions[0].type.parameters[0].signed, signed)
                self.reject(document(typedef(char.replace(f"signed {str(signed).lower()}",
                                                         f"signed {str(not signed).lower()}")),
                                     target=target, abi=abi))
                self.reject(document(function(function_type(), cc="other"), target=target, abi=abi), "target")
        for type_ in (
            INT.replace("rank 3", "rank 4"), INT.replace("bits 32", "bits 64"),
            INT.replace("signed true", "signed false"), INT.replace("size 4", "size unknown"),
            INT.replace("align 4", "align 8"), INT.replace("align 4", "align 3"),
            "(real long-double (format binary128) (bits 128) (size 16) (align 16))",
            "(real double (format binary64) (bits 64) (size 8) (align 16))",
            "(real float (format binary64) (bits 32) (size 4) (align 4))",
            pointer("void").replace("bits 64", "bits 32"),
        ):
            with self.subTest(type_=type_):
                self.reject(document(typedef(type_)))

    def test_unknown_and_duplicate_clauses_rejected(self):
        sources = (
            document("(abi \"SysVAMD64\")"), document("(sdk vendor)"), document("(macro \"X\")"),
            document(typedef(INT), typedef(INT)), document(typedef(INT, '"o"'), object_(INT)),
            document("(typedef \"T\" (type void) (type void))"),
            document(typedef(INT.replace("(align 4)", "(align 4) (align 4)"))),
            document(typedef(INT.replace("(align 4)", "(align 4) (extra unknown)"))),
            document("(typedef \"T\" (type void void))"),
            document("(typedef \"T\" (type))"), document("(typedef \"T\" ())"),
            document(typedef("(complex double)")), document("(typedef \"T\")"),
            document('(tag 0 struct "S" (complete false))', '(tag 0 union "U" (complete false))'),
            document(tag(0, field(INT), field(INT, offset=4), size=8)),
            document(f"(tag 0 enum none (complete true) (underlying {INT}) (enumerator \"a\" 1)"
                     "(enumerator \"a\" 1))"),
            document('(tag 0 struct "S" (complete false) (layout (size unknown) (align unknown)))'),
            document(f'(tag 0 enum "E" (complete true) (underlying {INT}) {field(INT)})'),
            document(f'(tag 0 enum "E" (complete true) (underlying {pointer(INT)}) (enumerator "a" 0))'),
            document(f'(tag 0 enum "E" (complete true) (underlying {INT}) (enumerator "a" 2147483648))'),
            document('(tag 0 struct none (complete true) (layout (size 4) (align 4)))'),
        )
        for source in sources:
            with self.subTest(source=source):
                self.reject(source)

    def test_definition_and_function_type_shapes(self):
        old = function(function_type(prototype="false"), name='"old"')
        empty = function(function_type(), name='"empty"')
        inline = function(function_type(INT, f"{INT} {INT}"), '"a" "a"', state="inline-only",
                          specs="inline noreturn", name='"inline"')
        doc = ci.decode_interface(document(old, empty, inline))
        self.assertFalse(doc.functions[0].type.prototype)
        self.assertTrue(doc.functions[1].type.prototype)
        self.assertEqual(doc.functions[2].parameter_names, ("a", "a"))
        self.assertEqual(doc.functions[2].specifiers, ("inline", "noreturn"))
        for decl in (
            function(function_type(params=INT, prototype="false"), "none"),
            function(function_type(variadic="true")), function(function_type(params="void"), "none"),
            function(function_type(params=array(INT)), "none"),
            function(function_type(params=function_type()), "none"),
            function(function_type(params=f"(qualified (qualifiers const) {INT})"), "none"),
            function(function_type(result=array(INT))), function(function_type(result=function_type())),
            function(function_type(), "none"), function(function_type(), state="tentative"),
            function(function_type(), state="inline-only"), function(function_type(), specs="noreturn noreturn"),
            function(function_type(), specs="extension"), function(INT), object_("void"),
            object_(function_type()), object_(INT, definition="inline-only"),
            function(function_type()).replace("(storage none)", "(storage)"),
            function(function_type()).replace("(storage none)", "(storage static)"),
            function(function_type()).replace("(storage none)", "(storage internal)"),
            typedef(f"(qualified (qualifiers const const) {INT})"),
            typedef(f"(qualified (qualifiers atomic) {INT})"),
            typedef(f"(qualified (qualifiers restrict) {INT})"),
            typedef(f"(qualified (qualifiers const) {function_type()})"),
            typedef(f"(qualified (qualifiers restrict) {pointer(function_type())})"),
        ):
            with self.subTest(decl=decl):
                self.reject(document(decl))
        incomplete = '(tag 0 struct "S" (complete false))'
        signature = function_type("(tag-ref 0)", "(tag-ref 0)")
        ci.decode_interface(document(incomplete, function(signature, "none")))
        self.reject(document(incomplete, function(signature, "none", state="defined")),
                    message="incomplete function definition")
        self.reject(document(incomplete,
                             function(function_type(params="(tag-ref 0)"), "none", state="defined")),
                    message="incomplete function definition")

    def test_dangling_tags_checked_behind_pointers_and_callbacks(self):
        for type_ in ("(tag-ref 0)", pointer("(tag-ref 0)"),
                      pointer(function_type(result="(tag-ref 0)"))):
            self.reject(document(typedef(type_)), message="dangling tag reference")

    def test_cycle_rejection_and_recursive_pointer_acceptance(self):
        direct = document(tag(0, field("(tag-ref 0)")))
        indirect = document(tag(0, field("(tag-ref 1)")),
                            tag(1, field(array("(tag-ref 0)", 1, 4))))
        qualified = document(tag(0, field("(qualified (qualifiers const) (tag-ref 0))")))
        for data in (direct, indirect, qualified):
            self.reject(data, message="cyclic by-value")
        doc = ci.decode_interface(document(
            tag(0, field("(tag-ref 77)"), size=8, align=8, name='"Recursive"'),
            tag(77, field(pointer("(tag-ref 0)")), size=8, align=8, name='"Recursive"'),
        ))
        self.assertEqual(doc.tags[0].fields[0].type, ci.TagRef(77))
        self.assertEqual(doc.tags[1].fields[0].type.pointee, ci.TagRef(0))

    def test_iterative_memoized_tag_walk_handles_long_chain_and_shared_dependencies(self):
        # Would exhaust Python's recursion limit if validated by recursive DFS.
        chain = [tag(i, field(f"(tag-ref {i + 1})")) for i in range(1700)] + [tag(1700, field(INT))]
        self.assertEqual(len(ci.decode_interface(document(*chain)).tags), 1701)
        chain[-1] = tag(1700, field("(tag-ref 0)"))
        self.reject(document(*chain), message="cyclic by-value")
        # Expanding nominal definitions would visit ~2**55 field occurrences.
        shared = [tag(55, field(INT))]
        for i in range(54, -1, -1):
            half = 4 << (54 - i)
            shared.append(tag(i, field(f"(tag-ref {i + 1})", '"left"'),
                              field(f"(tag-ref {i + 1})", '"right"', half), size=2 * half))
        self.assertEqual(len(ci.decode_interface(document(*reversed(shared))).tags), 56)

    def test_inconsistent_record_layouts_rejected(self):
        samples = (
            tag(0, field(INT), size=3), tag(0, field(INT), size=4, align=2),
            tag(0, field(INT, offset=2), size=8), tag(0, field(INT, offset=4), size=4),
            tag(0, field(INT, offset=4), size=8, kind="union"),
            tag(0, field(INT), field(INT, '"other"')),
            tag(0, field("(tag-ref 1)"), field(INT, '"visible"')),
            tag(0, field(INT, "none")),
            tag(0, field(INT, extra="(flexible-array true)")),
            tag(0, field(INT), field(array(INT), '"a"', 4)),
            tag(0, field(array(INT), '"a"', 0, "(flexible-array true)")),
            tag(0, field(INT), field(array(INT), '"a"', 4, "(flexible-array true)"), kind="union"),
            tag(0, field(INT), field(array(INT), '"a"', 4, "(flexible-array true)"),
                field(INT, '"b"', 4), size=8),
            tag(0, field(INT), field(array(INT), '"a"', 4, "(flexible-array false)")),
        )
        for item in samples:
            with self.subTest(item=item):
                self.reject(document(item, '(tag 1 struct "Incomplete" (complete false))'))

    def test_bit_field_bounds_include_only_occupied_bits(self):
        def bit_field(width, shift, container, offset=1, type_=UINT, name='"bits"'):
            return field(type_, name, offset, f"(bit-field (width {width}) (shift {shift}) "
                                             f"(container-offset {container}))")
        base = field(CHAR, '"first"')
        narrow = tag(0, base, bit_field(3, 0, 0, name="none"), field(CHAR, '"last"', 2),
                     size=3, align=1)
        decoded = ci.decode_interface(document(narrow))
        self.assertEqual(decoded.tags[0].fields[1].bit_field, ci.BitField(3, 0, 0))
        for bits in (bit_field(33, 0, 0), bit_field(25, 0, 0), bit_field(3, 8, 0),
                     bit_field(3, 0, 4), bit_field(3, 0, 1), bit_field(0, 0, 0),
                     bit_field(3, 0, 0, type_=CHAR), bit_field(3, 0, 0, offset=0)):
            self.reject(document(tag(0, base, bits)))

    def test_array_and_object_layouts(self):
        doc = ci.decode_interface(document(object_(array(INT), size="unknown")))
        self.assertEqual(doc.declarations[0].layout, ci.Layout(None, 4))
        for decl in (
            object_(INT, size=8), object_(INT, align=2), object_(INT, align="unknown"),
            object_(array(INT), size=0), object_(array(INT), size="unknown", definition="defined"),
            typedef(array(INT, 3, 8)), typedef(array(INT, 0, 0)), typedef(array(INT, -1, 4)),
            typedef(array(INT, 2, 8, 8)), typedef(array(INT, size=4)),
            typedef(array("void")), typedef(array(array(INT))),
            typedef(array(INT, (1 << 64) - 1, "unknown")),
            typedef(array("(tag-ref 0)", 1, 4)),
        ):
            with self.subTest(decl=decl):
                self.reject(document(decl, '(tag 0 struct "Incomplete" (complete false))'))

    def test_bounds_before_unbounded_conversion_or_expansion(self):
        self.reject(b" " * (ci.MAX_BYTES + 1), "limit", "byte limit")
        self.reject(b"(" * (ci.MAX_DEPTH + 1), "limit", "depth limit")
        self.reject(document(typedef(INT, '"' + "x" * (ci.MAX_STRING_BYTES + 1) + '"')),
                    "limit", "string byte limit")
        self.reject(document(typedef("x" * (ci.MAX_ATOM_BYTES + 1))), "limit", "atom byte limit")
        for value in ("1" * 5000, str(1 << 64), str(-(1 << 63) - 1)):
            self.reject(document(typedef(f"(tag-ref {value})")), "limit", "decimal")
        doc = ci.decode_interface(document(f'(tag {(1 << 64) - 1} struct "Max" (complete false))'))
        self.assertEqual(doc.tags[0].id, (1 << 64) - 1)
        for limit, value, forms, message in (
            ("MAX_TOKENS", 12, (), "token"),
            ("MAX_NODES", 5, (), "node"),
            ("MAX_LIST_ITEMS", 4, (), "list item"),
            ("MAX_DECLARATIONS", 1, (typedef(INT), typedef(INT, '"U"')), "declaration"),
            ("MAX_TAGS", 1, (tag(0, field(INT)), tag(1, field(INT))), "tag count"),
            ("MAX_MEMBERS", 1, (tag(0, field(INT), field(INT, '"n"', 4), size=8),), "tag member"),
            ("MAX_PARAMETERS", 1, (function(function_type(params=f"{INT} {INT}"), "none none"),),
             "parameter count"),
            ("MAX_REDECLARATIONS", 1,
             (object_(INT).replace("(storage extern)", "(storage extern extern)"),), "redeclaration"),
        ):
            with self.subTest(limit=limit), patch.object(ci, limit, value):
                self.reject(document(*forms), "limit", message)

    def test_error_does_not_echo_untrusted_text(self):
        for data in (document(typedef(INT, '"safe"'), '(SECRET "very long untrusted text")'),
                     b'(interface 1 (target "SECRET\xff") (data-model LP64) (abi "SysVAMD64"))'):
            with self.assertRaises(ci.InterfaceError) as failure:
                ci.decode_interface(data)
            self.assertNotIn("SECRET", str(failure.exception))


class _RealProjectionChecks:
    def test_three_qualified_targets_and_input_route_equivalence(self):
        for target, abi, convention, signed, fmt, width in TARGETS:
            with self.subTest(target=target):
                direct, preprocessed, syntax, sdk = tuple(
                    ci.decode_interface((W02_FIXTURES / f"{target}.{mode}.interface").read_bytes(),
                                        expected_target=target)
                    for mode in ("direct", "preprocessed", "syntax", "sdk")
                )
                self.assertEqual(direct, preprocessed)
                self.assertEqual(direct, syntax)
                self.assertEqual(direct, sdk)
                self.assertEqual(direct.abi, abi)
                named = {decl.name: decl for decl in direct.declarations}
                self.assertEqual(named["plain_char"].type, ci.IntegerType("char", 1, 8, signed, 1, 1))
                self.assertEqual(named["extended"].type, ci.RealType("long-double", fmt, width, 16, 16))
                self.assertEqual(named["apply"].calling_convention, convention)
                self.assertEqual(named["apply"].parameter_names, ("values", "cb"))
                self.assertEqual(named["inline_only"].definition, "inline-only")
                node = next(tag for tag in direct.tags if tag.name == "Node")
                self.assertEqual(node.layout, ci.Layout(40, 8))
                self.assertEqual(node.fields[3].type.pointee, ci.TagRef(node.id))
                self.assertTrue(node.fields[-1].flexible_array)
                self.assertIsNone(node.fields[-1].type.count)

    def test_real_clamp_header(self):
        for target, _, convention, _, _, _ in TARGETS:
            with self.subTest(target=target):
                raw = (W02_FIXTURES / f"clamp.{target}.interface").read_bytes()
                projected = ci.decode_interface(raw, expected_target=target)
                clamp = {f.name: f for f in projected.functions if f.name.startswith("mulib_clamp_")}
                self.assertEqual(set(clamp), {"mulib_clamp_add_i64", "mulib_clamp_sub_i64", "mulib_clamp_i64"})
                for fn in clamp.values():
                    self.assertEqual(fn.calling_convention, convention)
                    self.assertTrue(fn.type.prototype)
                    self.assertFalse(fn.type.variadic)
                    self.assertEqual(fn.type.result, ci.IntegerType("long", 4, 64, True, 8, 8))
                    self.assertEqual(fn.type.parameters,
                                     (fn.type.result,) * (3 if fn.name == "mulib_clamp_i64" else 2))

    def test_real_internal_inline_function(self):
        for target, _, convention, _, _, _ in TARGETS:
            with self.subTest(target=target):
                raw = (W02_FIXTURES / f"container-core.{target}.interface").read_bytes()
                projected = ci.decode_interface(raw, expected_target=target)
                fn = next(f for f in projected.functions if f.name == "mulib_ct_counter_add")
                self.assertEqual((fn.linkage, fn.definition, fn.storage, fn.specifiers, fn.calling_convention),
                                 ("internal", "defined", ("static",), ("inline",), convention))
                uint64 = ci.IntegerType("unsigned-long", 4, 64, False, 8, 8)
                self.assertEqual(fn.type, ci.FunctionType(True, False, uint64, (uint64, uint64)))

    def test_projection_does_not_imply_body_validity(self):
        for target, _, _, _, _, _ in TARGETS:
            with self.subTest(target=target):
                raw = (W02_FIXTURES / f"{target}.body-declaration-only.interface").read_bytes()
                projected = ci.decode_interface(raw, expected_target=target)
                self.assertEqual(len(projected.declarations), 1)
                self.assertEqual(projected.functions[0].definition, "defined")
                stderr = (W02_FIXTURES / f"{target}.body-not-lowerable.stderr").read_bytes()
                self.assertIn(b"variable-length arrays", stderr)


def load_tests(loader, _tests, _pattern):
    tests = loader.loadTestsFromTestCase(ReaderTests)
    if W02_FIXTURES is not None:
        class RealProjectionTests(_RealProjectionChecks, unittest.TestCase):
            pass

        tests.addTests(loader.loadTestsFromTestCase(RealProjectionTests))
    return tests


if __name__ == "__main__":
    cli = argparse.ArgumentParser(add_help=False)
    cli.add_argument("--w02-fixtures", type=Path)
    args, remaining = cli.parse_known_args()
    W02_FIXTURES = args.w02_fixtures
    unittest.main(argv=[sys.argv[0], *remaining])
