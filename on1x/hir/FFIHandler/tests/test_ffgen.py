"""W03 generation policy tests; optional real-tool captures are explicit inputs."""
import base64
from dataclasses import replace
import json
import os
from pathlib import Path
from types import SimpleNamespace
import unittest

from cephyr_interface import (
    Field, Function, FunctionType, IntegerType, Interface, Layout, Object,
    PointerType, QualifiedType, RealType, Tag, TagRef, VoidType, decode_interface,
)
import ffic
from ffgen import (
    GenerationError, GuardDelta, Override, _check_public_header, _parser_identity, failed_module,
    generate_module,
)
from ffi_types import reachable_native_graph, target_identity


TARGET = target_identity("x86_64-linux-gnu")
OTHER_TARGET = target_identity("aarch64-linux-gnu")
WORD = IntegerType("long", 4, 64, True, 8, 8)
INT = IntegerType("int", 3, 32, True, 4, 4)
UNSIGNED = IntegerType("unsigned-int", 3, 32, False, 4, 4)
BOOL = IntegerType("bool", 0, 1, False, 1, 1)
FLOAT = RealType("float", "binary32", 32, 4, 4)
DOUBLE = RealType("double", "binary64", 64, 8, 8)


def entry(key="x", *, implementation="switch"):
    alias = key.replace("-", "_")
    return SimpleNamespace(key=key, module="mulib." + alias, filename=key + ".ffi",
                           header=f"mulib-{key}.h", header_path=f"include/mulib-{key}.h",
                           implementation=implementation, reserved_prefix=f"mulib_{alias}_")


RECIPE = entry()


def function(name, params=(), result=WORD, *, names=None, prototype=True, variadic=False,
             linkage="external", definition="declaration", specifiers=()):
    return Function(name, FunctionType(prototype, variadic, result, params), linkage, ("none",),
                    definition, TARGET[3], (None,) * len(params) if names is None else names,
                    specifiers)


def model(*declarations, tags=()):
    return Interface(1, *TARGET[:3], declarations, tags)


def generate(*declarations, tags=(), **kwargs):
    return generate_module(RECIPE, model(*declarations, tags=tags), TARGET, **kwargs)


def owned(*names):
    return {name: (RECIPE.header_path,) for name in names}


def symbols(outcome):
    return {row["symbol"]: row for row in outcome.metadata["symbols"]}


class GenerationTests(unittest.TestCase):
    def test_no_prefix_only_export_or_dependency_leak(self):
        document = generate(
            function("printf"), function("malloc"), function("mulib_dependency_f"),
            function("mulib_x_unattributed"), function("mulib_x_foreign_only"),
            function("mulib_x_multi_origin"), function("mulib_x_dep_competing"),
            function("mulib_x_attributed"),
            Object("mulib_x_count", INT, "external", ("extern",), "declaration", Layout(4, 4)),
            origins={
                "mulib_x_foreign_only": ("include/mulib-dep.h",),
                "mulib_x_multi_origin": (RECIPE.header_path, "include/mulib-dep.h"),
                **owned("mulib_x_dep_competing", "mulib_x_attributed"),
            },
            reserved_prefixes=("mulib_x_", "mulib_x_dep_"),
        )
        self.assertIn(b"fn attributed(", document.document)
        for symbol in (b"printf", b"malloc", b"mulib_dependency_f", b"mulib_x_count",
                       b"mulib_x_foreign_only", b"mulib_x_dep_competing"):
            self.assertNotIn(symbol, document.document)
        rows = symbols(document)
        self.assertEqual(rows["mulib_x_unattributed"]["reason"], "ambiguous_export")
        self.assertEqual(rows["mulib_x_multi_origin"]["reason"], "ambiguous_export")
        self.assertEqual(rows["mulib_x_dep_competing"]["reason"], "reserved_other_module")
        self.assertEqual(rows["mulib_x_foreign_only"]["reason"], "foreign_origin")

    def test_public_header_must_resolve_to_manifest_path(self):
        root = Path("/fixture")
        canonical = root / RECIPE.header_path
        seen = []

        def selected(path):
            def check(header):
                seen.append(header)
                return path
            return SimpleNamespace(check_public_header=check)

        _check_public_header(root, RECIPE, selected(canonical))
        with self.assertRaisesRegex(GenerationError, "not the canonical manifest header"):
            _check_public_header(root, RECIPE, selected(root / "sysroot" / RECIPE.header))
        self.assertEqual(seen, [RECIPE.header, RECIPE.header])

    def test_parser_provenance_elides_paths_but_binds_grammar_and_parser(self):
        grammar = "a" * 64

        def artifact(path, *, value="token", code=b"MODE = 'interpret'\n"):
            payload = base64.b64encode(json.dumps({
                "file": path, "dependencies": [{"path": path, "sha256": grammar}],
                "tokens": [value],
            }).encode("ascii"))
            return b'SPEC = json.loads(base64.b64decode("' + payload + b'"))\n' + code

        first = _parser_identity(artifact("/a/grammar"), grammar)
        self.assertEqual(first, _parser_identity(artifact("/b/grammar"), grammar))
        for changed in (artifact("/a/grammar", value="other"), artifact("/a/grammar", code=b"")):
            self.assertNotEqual(first, _parser_identity(changed, grammar))
        with self.assertRaisesRegex(GenerationError, "does not bind"):
            _parser_identity(artifact("/a/grammar"), "0" * 64)

    def test_multiple_owned_headers_must_be_explicit(self):
        fn = function("mulib_x_custom")
        origins = {fn.name: (RECIPE.header_path, "include/mulib-x-detail.h")}
        self.assertIn(b"ambiguous_export", generate(fn, origins=origins).document)
        result = generate(fn, origins=origins, owned_headers=("include/mulib-x-detail.h",))
        self.assertIn(b"fn custom(", result.document)

    def test_non_scalar_prototypes_effects_and_linkage(self):
        cases = [
            (function("mulib_x_proto", prototype=False), "no_prototype"),
            (function("mulib_x_var", (INT,), variadic=True), "variadic"),
            (function("mulib_x_inline", definition="inline-only", specifiers=("inline",)),
             "external_inline_only"),
            (function("mulib_x_helper", linkage="internal", definition="defined"), "internal_linkage"),
            (function("mulib_x_exit", specifiers=("noreturn",)), "noreturn"),
            (function("mulib_x_volatile", result=QualifiedType(("volatile",), INT)),
             "qualified_type"),
            (function("mulib_x_char", result=IntegerType("char", 1, 8, True, 1, 1)), "plain_char"),
            (function("mulib_x_ld", result=RealType("long-double", "x87", 80, 16, 16)),
             "long_double"),
            (function("mulib_x_wide", (INT,) * 17), "parameter_limit"),
        ]
        output = generate(*(fn for fn, _ in cases), origins=owned(*(fn.name for fn, _ in cases)))
        self.assertEqual(output.metadata["status"], "unsupported")
        for fn, reason in cases:
            self.assertEqual(symbols(output)[fn.name]["reason"], reason)
        self.assertNotIn(b"profile scalar", output.document)

    def test_scalar_surface_does_not_change_native_identity(self):
        native = (INT, WORD, replace(WORD, name="long-long", rank=5), UNSIGNED,
                  BOOL, FLOAT, DOUBLE, IntegerType("unsigned-char", 1, 8, False, 1, 1),
                  IntegerType("unsigned-short", 2, 16, False, 2, 2),
                  IntegerType("unsigned-long", 4, 64, False, 8, 8))
        fn = function("mulib_x_scalars", native, VoidType())
        output = generate(fn, origins=owned(fn.name))
        row = symbols(output)[fn.name]
        self.assertEqual(row["surface"], {
            "parameters": ["i64", "i64", "i64", "u32", "bool", "f32", "f64", "u8", "u16", "u64"],
            "result": "void",
        })
        self.assertIn(b"integer(int,3,32,signed,4,4)", output.document)
        self.assertIn(b"integer(long-long,5,64,signed,8,8)", output.document)
        self.assertIn(b"-> void profile scalar", output.document)
        self.assertNotIn(b"safe", output.document)

    def test_target_mismatch_is_invalid_not_discovery(self):
        fn = function("mulib_x_count")
        with self.assertRaisesRegex(ValueError, "target/data model/ABI"):
            generate_module(RECIPE, model(fn), OTHER_TARGET)
        with self.assertRaisesRegex(ValueError, "calling convention"):
            generate(replace(fn, calling_convention="aapcs64"), origins=owned(fn.name))

    def test_override_uses_position_and_ignores_parameter_renames(self):
        fn = function("mulib_x_bound", (WORD, WORD, WORD, WORD),
                      names=(None, "arg0", "dup", "dup"))
        first = generate(fn, origins=owned(fn.name))
        row = symbols(first)[fn.name]
        self.assertEqual(row["original_parameter_names"], [None, "arg0", "dup", "dup"])
        self.assertEqual(row["parameter_names"], ["arg0_1", "arg0", "arg2", "arg3"])
        override = Override(TARGET, RECIPE.module, (fn.name,), {
            fn.name: GuardDelta(row["native_fingerprint"], ((1, 3),)),
        })
        changed = generate(replace(fn, parameter_names=("renamed", None, "low", "hi")),
                           override=override)
        self.assertIn(b"requires 1 <= 3", changed.document)
        self.assertEqual(first.metadata["normalized_signature_sha256"],
                         changed.metadata["normalized_signature_sha256"])
        self.assertEqual(row["native_fingerprint"], symbols(changed)[fn.name]["native_fingerprint"])
        same_width_change = replace(fn, type=replace(
            fn.type, parameters=(replace(WORD, name="long-long", rank=5), WORD, WORD, WORD)))
        with self.assertRaisesRegex(GenerationError, "stale native signature"):
            generate(same_width_change, override=override)
        with self.assertRaisesRegex(GenerationError, "stale native signature"):
            generate(replace(fn, definition="defined"), override=override)

    def test_stale_or_invalid_override_never_grants_eligibility(self):
        fn = function("mulib_x_f", (INT, FLOAT, INT, UNSIGNED))
        fp = symbols(generate(fn, origins=owned(fn.name)))[fn.name]["native_fingerprint"]
        for pair, reason in [((0, 4), "guard position"), ((True, 2), "guard position"),
                             ((0, 1), "matching integral"), ((0, 3), "matching integral")]:
            with self.subTest(pair=pair), self.assertRaisesRegex(GenerationError, reason):
                generate(fn, override=Override(TARGET, RECIPE.module, (fn.name,), {
                    fn.name: GuardDelta(fp, (pair,)),
                }))
        with self.assertRaisesRegex(GenerationError, "stale or ineligible override"):
            generate(fn, override=Override(TARGET, RECIPE.module, (), {
                fn.name: GuardDelta(fp, ((0, 2),)),
            }))
        with self.assertRaisesRegex(GenerationError, "stale or unowned selection"):
            generate(fn, origins={fn.name: ("include/dependency.h",)},
                     override=Override(TARGET, RECIPE.module, (fn.name,)))
        with self.assertRaisesRegex(GenerationError, "stale or unowned selection"):
            generate(fn, origins={fn.name: (RECIPE.header_path, "include/dependency.h")},
                     override=Override(TARGET, RECIPE.module, (fn.name,)))
        with self.assertRaisesRegex(GenerationError, "stale or unowned selection"):
            generate(fn, override=Override(TARGET, RECIPE.module, ("puts",)))
        with self.assertRaisesRegex(GenerationError, "target/module mismatch"):
            generate(fn, override=Override(OTHER_TARGET, RECIPE.module))
        with self.assertRaisesRegex(GenerationError, "target/module mismatch"):
            generate(fn, override=Override(TARGET, "mulib.other"))

    def test_recursive_and_same_named_tags_preserve_nominal_distinction(self):
        def capture(first_id, second_id, *, merge=False):
            ptr_first = PointerType(64, 8, 8, TagRef(first_id))
            ptr_second = PointerType(64, 8, 8, TagRef(first_id if merge else second_id))
            tags = [
                Tag(second_id, "struct", "Item", False, None, (), None, ()),
                Tag(first_id, "struct", "Item", True, Layout(8, 8),
                    (Field("next", 0, ptr_first, None, False),), None, ()),
            ]
            fns = [function("mulib_x_second", (ptr_second,)),
                   function("mulib_x_first", (ptr_first,))]
            return generate(*fns, tags=tuple(tags), origins=owned(*(f.name for f in fns)))

        first, reordered, merged = capture(0, 5), capture(98, 31), capture(0, 5, merge=True)
        self.assertEqual(first, reordered)
        self.assertNotEqual(first.metadata["normalized_signature_sha256"],
                            merged.metadata["normalized_signature_sha256"])
        tags = first.metadata["native_tags"]
        self.assertEqual([tag["id"] for tag in tags], [0, 1])
        self.assertEqual(tags[0]["fields"][0]["type"][-1], ["tag-ref", 0])
        self.assertIsNone(tags[1]["layout"])
        self.assertNotIn(b"profile scalar", first.document)

    def test_rich_graph_ordinals_are_symbol_local_and_result_first(self):
        def capture(result_id, arg_id, unrelated_id, unrelated_name):
            result = PointerType(64, 8, 8, TagRef(result_id))
            arg = PointerType(64, 8, 8, TagRef(arg_id))
            fn = function("mulib_x_graph", (arg,), result)
            return reachable_native_graph(fn, (
                Tag(unrelated_id, "struct", unrelated_name, False, None, (), None, ()),
                Tag(arg_id, "struct", "Node", False, None, (), None, ()),
                Tag(result_id, "struct", "Node", True, Layout(8, 8),
                    (Field("next", 0, result, None, False),), None, ()),
            ))

        root, tags = capture(30, 10, 0, "First")
        self.assertEqual((root, tags), capture(2, 99, 1, "Renamed"))
        self.assertEqual(root[3][-1], ["tag-ref", 0])
        self.assertEqual(root[4][0][-1], ["tag-ref", 1])
        self.assertEqual([(tag["id"], tag["complete"]) for tag in tags], [(0, True), (1, False)])
        self.assertEqual(tags[0]["fields"][0]["type"][-1], ["tag-ref", 0])

    def test_failed_module_preserves_discovery_obligations_but_not_stale_selection(self):
        obligations = Override(TARGET, RECIPE.module, obligations=("retention_unknown",))
        failed = failed_module(RECIPE, TARGET, "cephyr_failed", override=obligations)
        self.assertEqual(failed.metadata["obligations"], ["retention_unknown"])
        self.assertIn(b"unavailable reason cephyr_failed", failed.document)
        self.assertNotIn(b" fn ", failed.document)
        with self.assertRaisesRegex(GenerationError, "cannot verify override"):
            failed_module(RECIPE, TARGET, "cephyr_failed",
                          override=replace(obligations, selections=("mulib_x_one",)))

    def test_module_budget_is_explicit_and_does_not_lose_overrides(self):
        fns = [function(f"mulib_x_fn_{n:03}") for n in range(257)]
        origins = owned(*(fn.name for fn in fns))
        output = generate(*fns, origins=origins)
        self.assertIn(b"unavailable reason module_export_limit;", output.document)
        self.assertFalse(any(row["emitted"] for row in output.metadata["symbols"]))
        single = function("mulib_x_checked", (INT, INT))
        fp = symbols(generate(single, origins=owned(single.name)))[single.name]["native_fingerprint"]
        with self.assertRaisesRegex(GenerationError, "cannot emit authored override"):
            generate(single, *fns, origins=origins, override=Override(TARGET, RECIPE.module,
                     (single.name,), {single.name: GuardDelta(fp, ((0, 1),))}))


@unittest.skipUnless(os.environ.get("W03_CEPHYR_FIXTURES") and os.environ.get("W03_LLSYNTH_PARSER"),
                     "set W03_CEPHYR_FIXTURES and W03_LLSYNTH_PARSER for real-tool qualification")
class RealCaptures(unittest.TestCase):
    def projection(self, key, target):
        fixture = Path(os.environ["W03_CEPHYR_FIXTURES"]) / f"{key}.{target[0]}.interface"
        return decode_interface(fixture.read_bytes(), expected_target=target[0])

    def validate(self, output):
        parser = ffic.load_parser(Path(os.environ["W03_LLSYNTH_PARSER"]))
        modules = list(ffic.parse_document(parser, output.document, "w03-generated.ffi"))
        self.assertEqual(len(modules), 1)
        return modules[0]

    def test_clamp_projects_all_targets_and_merges_real_position_override(self):
        module = entry("clamp")
        for triple in ("x86_64-linux-gnu", "aarch64-linux-gnu", "riscv64-linux-gnu"):
            with self.subTest(triple=triple):
                target = target_identity(triple)
                projection = self.projection("clamp", target)
                selections = tuple(fn.name for fn in projection.functions
                                   if fn.name in {"mulib_clamp_i64", "mulib_clamp_add_i64",
                                                  "mulib_clamp_sub_i64"})
                data = generate_module(module, projection, target,
                                       override=Override(target, module.module, selections))
                fp = symbols(data)["mulib_clamp_i64"]["native_fingerprint"]
                parser = ffic.load_parser(Path(os.environ["W03_LLSYNTH_PARSER"]))
                override_src = f"""ffi 2;
target "{triple}" {target[1]} "{target[2]}" {target[3]};
extend mulib.clamp {{
    select c::mulib_clamp_add_i64;
    select c::mulib_clamp_sub_i64;
    select c::mulib_clamp_i64;
    fn c::mulib_clamp_i64 fingerprint "{fp}" requires 1 <= 2;
}}
""".encode("ascii")
                from ffgen import _parse_override
                override = _parse_override(parser, override_src, "w03-test-clamp.ffi")
                output = generate_module(module, projection, target, override=override)
                parsed = self.validate(output)
                self.assertEqual(len(parsed["entries"]), 3)
                row = next(row for row in parsed["entries"] if row["symbol"] == "mulib_clamp_i64")
                self.assertEqual(row["guards"], [[1, 2]])
                self.assertEqual(row["native_result"], ["integer", "long", 4, 64, "signed", 8, 8])

    def test_real_inline_is_an_exact_selection_exception_not_a_prefix(self):
        module = entry("container-core", implementation="static-inline")
        projection = self.projection("container-core", TARGET)
        symbol = "mulib_ct_counter_add"
        result = generate_module(module, projection, TARGET,
                                 override=Override(TARGET, module.module, (symbol,)))
        parsed = self.validate(result)
        self.assertEqual([(row["member"], row["symbol"]) for row in parsed["entries"]],
                         [("counter_add", symbol)])
        self.assertEqual(parsed["entries"][0]["linkage"], "internal")
        self.assertEqual(parsed["entries"][0]["result"], "u64")
        without_selection = generate_module(module, projection, TARGET)
        self.assertNotIn(b"mulib_ct_", without_selection.document)


if __name__ == "__main__":
    unittest.main()
