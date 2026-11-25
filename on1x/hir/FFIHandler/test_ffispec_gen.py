"""Exercise the automatic generator, its proof, and its real Cephyr inputs."""

import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from compile_recipes import prove_i64_body
from generate_portable_recipe import MAX_I64, MIN_I64, Rejection
from kernel_inputs import Inputs, implementation_macro, roots_for
from recipe_mir import directives, read_mir, verified_evidence
from recipe_totality import prove_totality


SCRIPT = Path(__file__).resolve().with_name("ffispec-gen.py")
ROOT = SCRIPT.parents[3]
LIBRARY = os.environ.get("CEPHYR_LIBRARY")


class Totality(unittest.TestCase):
    def test_proves_affine_paths_and_short_circuit_without_guessing_guards(self):
        prove_totality(("sub", "$0", "$0"), 1)
        prove_totality(("if", ("lt", "$0", MAX_I64), ("add", "$0", 1), "$0"), 1)
        prove_totality(("if", ("and", ("gt", "$1", 0), ("gt", "$0", ("sub", MAX_I64, "$1"))),
                        MAX_I64,
                        ("if", ("or", ("ge", "$1", 0), ("ge", "$0", ("sub", MIN_I64, "$1"))),
                         ("add", "$0", "$1"), MIN_I64)), 2)
        # Proving a relation may require eliminating a variable, not just
        # looking up the exact affine expression in the condition.
        prove_totality(("if", ("and", ("eq", "$0", "$1"), ("lt", "$1", MAX_I64)),
                        ("add", "$0", 1), "$0"), 2)

    def test_direct_equality_peer_bounds_agree_with_independent_totality(self):
        x, y = "$0", "$1"
        eq, upper, lower = ("eq", x, y), ("lt", y, MAX_I64), ("gt", y, MIN_I64)
        for condition, value in [
            (("and", eq, upper), ("add", x, 1)),
            (("and", upper, ("eq", y, x)), ("add", x, 1)),
            (("and", eq, lower), ("sub", x, 1)),
            (("and", eq, lower), ("sub", 0, x)),
        ]:
            body = ("if", condition, value, x)
            with self.subTest(body=body):
                prove_totality(body, 2)
                prove_i64_body(body)

        bump = ("add", x, 1)
        for body in [
            ("if", eq, bump, x),  # equality alone cannot exclude INT64_MAX
            ("if", ("and", eq, ("le", y, MAX_I64)), bump, x),
            ("if", upper, ("if", eq, x, bump), x),  # upper bound on y, but x != y
            ("if", ("and", upper, ("eq", x, ("add", y, 1))), bump, x),  # x-y == 1
            ("if", ("and", upper, ("le", y, x)), bump, x),  # x-y includes zero, but is not zero
            ("if", ("or", eq, upper), bump, x),
            ("if", ("and", eq, ("ge", y, MIN_I64)), ("sub", 0, x), x),
        ]:
            with self.subTest(body=body):
                with self.assertRaisesRegex(Rejection, "cannot prove"):
                    prove_totality(body, 2)
                with self.assertRaisesRegex(ValueError, "cannot prove"):
                    prove_i64_body(body)

    def test_unreachable_rhs_only_is_safe(self):
        safe = ("if", ("and", ("lt", "$0", MAX_I64), ("eq", ("add", "$0", 1), 4)), 1, 0)
        prove_totality(safe, 1)
        for body in [
            ("add", "$0", 1), ("sub", MIN_I64, "$0"),
            ("if", ("and", safe[1][2], safe[1][1]), 1, 0),
            ("if", ("or", ("lt", "$0", MAX_I64), safe[1][2]), 1, 0),
            ("if", ("ge", "$0", 0), ("sub", 0, "$0"), ("sub", 0, "$0")),
        ]:
            with self.subTest(body=body), self.assertRaisesRegex(Rejection, "cannot prove"):
                prove_totality(body, 1)


MIR = """\
archon-mir 1
module 1 0 0
function sample i64 1 5 1 1
types i64 ptr i64 i64 i64
local 8 8
block 0 5
params
frame_addr 1 0
store 1 0 0
load 2 1 0
const 3 0
add 4 2 3
ret 4
end
"""


class MirEvidence(unittest.TestCase):
    def test_typed_stack_and_identity_produce_only_scoped_advisory_requests(self):
        evidence = verified_evidence(read_mir(MIR)["sample"], 1)
        self.assertEqual(evidence, {"local_loads": 1, "local_stores": 1, "identity_sites": 1})
        self.assertEqual(directives("aarch64-linux-gnu", evidence), [
            "(hint local-memory (backend mir-archon) (target aarch64-unknown-linux-gnu) (speculative))",
            "(rewrite fold-identity (backend mir-archon) (target aarch64-unknown-linux-gnu) (speculative))",
        ])
        self.assertEqual(directives("unrecognized", evidence), [])

    def test_malformed_or_nonlocal_facts_cannot_enable_hints(self):
        for before, after in [
            ("store 1 0 0\nload 2 1 0", "load 2 1 0\nstore 1 0 0"),
            ("load 2 1 0", "load 2 1 8"),
            ("load 2 1 0", "load 2 0 0"),
            ("add 4 2 3", "add 4 2 4"),
            ("add 4 2 3", "add 2 2 3"),
            ("ret 4", "br 0 0"),
            ("ret 4", "trap"),
            ("end", "end malicious"),
            ("module 1 0 0", "module 0 0 0"),
        ]:
            with self.subTest(after=after), self.assertRaises(Rejection):
                verified_evidence(read_mir(MIR.replace(before, after))["sample"], 1)


class Discovery(unittest.TestCase):
    def test_ordered_paths_and_dependency_delta(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            first, second = root / "first", root / "second"
            first.mkdir()
            second.mkdir()
            alias = root / "alias"
            alias.symlink_to(first, target_is_directory=True)
            self.assertEqual(roots_for(f":{alias}::{first}:{second}:", root), [first, second])
            self.assertEqual(roots_for("::", second), [second])
            header = first / "mulib-demo.h"
            header.write_bytes(b'#include <constant.h>\n')
            old = Inputs([first, second]).fingerprint(header)
            dep = second / "constant.h"
            dep.write_bytes(b'#define VALUE 1\n')
            current = Inputs([first, second]).fingerprint(header)
            self.assertNotEqual(old, current)
            self.assertEqual(current[2], {header, dep})
            self.assertTrue(current[1])
            dep.write_bytes(b'#line 1 "/fake.h"\n')
            self.assertFalse(Inputs([first, second]).fingerprint(header)[1])
        self.assertEqual(implementation_macro("mulib-demo.h", b"""
/* #if defined(MULIB_IMPL_DECOY) */
#if defined(\\
  MULIB_IMPL_REAL)
#endif
"""), ("MULIB_IMPL_REAL", ()))


@unittest.skipUnless(LIBRARY, "set CEPHYR_LIBRARY for real Cephyr integration tests")
class Generator(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.first, self.second = self.root / "first", self.root / "second"
        self.first.mkdir()
        self.second.mkdir()
        self.output = self.root / "output"
        self.paths = f":{self.first}::{self.second}:"

    def write(self, relative, text, root=None):
        path = (root or self.first) / relative
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_text(text, encoding="utf-8")
        return path

    def generate(self, *options, expected=0):
        result = subprocess.run([sys.executable, "-B", str(SCRIPT), "--output-dir", str(self.output),
                                 "--cephyr-library", LIBRARY, *options],
                                env={**os.environ, "MULIB_KERNELS_PATH": self.paths},
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, expected, result.stderr)
        if expected != 0:
            return result
        index = json.loads((self.output / "index.json").read_bytes())
        self.assertEqual(index["version"], 1)
        self.assertTrue(index["cephyr_version"])
        return index

    def test_emits_fixture_and_clamp_without_review_or_synthetic_api_guard(self):
        for source in [ROOT / "kernels/mulib-clamp.h", SCRIPT.parent / "tests/mulib-ffifixture.h"]:
            (self.first / source.name).write_bytes(source.read_bytes())
        idx = self.generate()
        self.assertEqual([entry["file"] for entry in idx["recipes"]], ["recipes/clamp.ffi", "recipes/ffifixture.ffi"])
        for entry, names in zip(idx["recipes"], [["add_i64", "i64", "sub_i64"], ["bump", "floor", "zero"]]):
            with self.subTest(entry=entry["kernel"]):
                self.assertEqual([export["member"] for export in entry["exports"]], names)
                self.assertEqual(Path(entry["header"]).parent, self.first)
                text = (self.output / entry["file"]).read_text(encoding="ascii")
                self.assertEqual(text.count("(guard true)"), 3)
                self.assertNotIn("(hint kernel-hot", text)
                self.assertNotIn("(hint private-code", text)
                self.assertEqual(entry["complete_profile"], "implementation")
                self.assertEqual({stage for stage, data in entry["attempts"]["implementation"].items()
                                  if data["status"] == "available"}, {"ast", "interface", "mir"})

    def test_emits_equality_bounded_body_but_catalogues_unbounded_peer(self):
        self.write("mulib-peer.h", """\
long mulib_peer_bump(long x, long y) {
    if (x == y && y < 9223372036854775807L) return x + 1L;
    return x;
}
""")
        self.write("mulib-unbounded.h", """\
long mulib_unbounded_bump(long x, long y) {
    if (x == y) return x + 1L;
    return x;
}
""")
        index = self.generate()
        entry, = index["recipes"]
        self.assertEqual(entry["file"], "recipes/peer.ffi")
        recipe = (self.output / entry["file"]).read_text(encoding="ascii")
        self.assertIn("(guard true)", recipe)
        self.assertIn("(body (if (and (eq x y) (lt y 9223372036854775807)) (add x 1) x))", recipe)
        rejected = next(header for header in index["headers"] if header["kernel"] == "unbounded")
        self.assertEqual(rejected["status"], "unsupported")
        catalogue = (self.output / rejected["file"]).read_text()
        self.assertIn("cannot prove signed i64 add", catalogue)

    def test_catalogues_distinguish_unsupported_malformed_and_missing_views(self):
        self.write("detail/other header.h", "extern int count;\n")
        self.write("mulib-unsafe.h", "long add(long x) { return x + 1L; }\n")
        self.write("mulib-pointer.h", "const char *text(void) { return 0; }\n")
        self.write("mulib-broken.h", '#include "does-not-exist.h"\n')
        self.write('mulib-"nonrepresentable.h', "long valid(void) { return 0; }\n")
        self.write("mulib-conflict.h", "int example(int);\ndouble example(int);\n")
        self.write("mulib-fallback.h", """\
long work(void) { return 1; }
#ifdef MULIB_IMPL_DIFFERENT_NAME
#include "implementation-only.h"
#endif
""")
        index = self.generate()
        self.assertEqual(index["recipes"], [])
        by_name = {entry["relative"]: entry for entry in index["headers"]}
        for name, status in {
            'mulib-"nonrepresentable.h': "malformed-input", "mulib-broken.h": "missing-views",
            "mulib-unsafe.h": "unsupported", "mulib-pointer.h": "unsupported",
            "mulib-conflict.h": "missing-views", "mulib-fallback.h": "missing-views",
            "detail/other header.h": "unsupported",
        }.items():
            with self.subTest(name=name):
                entry = by_name[name]
                self.assertEqual(entry["status"], status)
                self.assertTrue((self.output / entry["file"]).read_bytes().startswith(b"(ffi-catalogue 1"))
        fallback = by_name["mulib-fallback.h"]
        self.assertEqual(fallback["complete_profile"], "declarations")
        self.assertEqual(fallback["implementation_macro"], "MULIB_IMPL_DIFFERENT_NAME")
        catalog = (self.output / by_name["mulib-unsafe.h"]["file"]).read_text()
        self.assertIn("cannot prove signed i64 add", catalog)
        self.assertIn("(ast\n(1 (object", catalog)
        self.assertIn("(interface\n(interface 1", catalog)
        self.assertIn('(mir\n"archon-mir 1\\n', catalog)

    def test_shadowing_stale_cleanup_and_transitive_input_mutation(self):
        upper = self.write("mulib-demo.h", '#include "constant.h"\nlong mulib_demo_value(void) { return VALUE; }\n')
        lower = self.write("detail/mulib-demo.h", "long mulib_demo_value(void) { return 9; }\n", self.second)
        dependency = self.write("constant.h", "#define VALUE 1\n", self.second)
        entry, = self.generate()["recipes"]
        self.assertEqual(entry["header"], str(upper))
        self.assertEqual(entry["shadowed"], [str(lower)])
        recipe = self.output / entry["file"]
        index = self.output / "index.json"
        before = (recipe.stat().st_mtime_ns, index.stat().st_mtime_ns)
        self.assertIn("(body 1)", recipe.read_text())
        self.assertEqual(self.generate()["recipes"], [entry])
        self.assertEqual((recipe.stat().st_mtime_ns, index.stat().st_mtime_ns), before)
        dependency.write_text("#define VALUE 7\n")
        self.generate()
        self.assertIn("(body 7)", recipe.read_text())
        upper.unlink()
        new, = self.generate()["recipes"]
        self.assertEqual(new["header"], str(lower))
        self.assertIn("(body 9)", recipe.read_text())
        lower.write_text("long mulib_demo_value(long x) { return x + 1L; }\n")
        self.assertEqual(self.generate()["recipes"], [])
        catalogue = self.output / "catalogue/detail/mulib-demo.ffi"
        self.assertFalse(recipe.exists())
        self.assertTrue(catalogue.is_file())
        lower.unlink()
        self.generate()
        self.assertFalse(catalogue.exists())

    def test_same_translation_unit_and_cache_skips_cephyr(self):
        self.write("mulib-unit.h", "long mulib_unit_zero(void) { return 0; }\n")
        generator = runpy.run_path(str(SCRIPT))
        sys.path.insert(0, str(generator["CEPHYR"] / "python"))
        self.addCleanup(sys.path.remove, sys.path[0])
        from cephyr import Compiler

        compiler = Compiler(LIBRARY)
        with patch.object(compiler, "parse_c11", wraps=compiler.parse_c11) as ast, \
             patch.object(compiler, "emit_interface", wraps=compiler.emit_interface) as interface, \
             patch.object(compiler, "emit_mir", wraps=compiler.emit_mir) as mir:
            for _ in range(2):
                self.output.mkdir(exist_ok=True)
                generator["generate"](compiler, generator["CEPHYR_CONFIG"], [self.first], self.output, LIBRARY)
            self.assertEqual(ast.call_count, 2)  # infrastructure probe and the uncached header
            self.assertTrue((self.output / "recipes/unit.ffi").is_file())
            self.assertEqual([call.args for call in ast.call_args_list], [call.args for call in interface.call_args_list])
            self.assertEqual([call.args for call in ast.call_args_list], [call.args for call in mir.call_args_list])
            self.assertTrue(all(call.kwargs["format"] == "sexp" for call in ast.call_args_list))

    def test_invalid_config_is_fatal(self):
        self.write("mulib-unit.h", "long mulib_unit_zero(void) { return 0; }\n")
        config = self.root / "Cephyr.toml"
        config.write_text("misspelled_key = true\n")
        result = self.generate("--config", str(config), expected=1)
        self.assertIn("Cephyr preflight failed", result.stderr)
        self.assertFalse((self.output / "index.json").exists())

    def test_identity_directive_comes_from_actual_target_mir(self):
        self.write("mulib-identity.h", "long mulib_identity_add(long x) { return x + 0L; }\n")
        config = self.root / "Cephyr.toml"
        config.write_text('target = "aarch64-linux-gnu"\n')
        entry, = self.generate("--config", str(config))["recipes"]
        self.assertEqual(entry["cephyr_target"], "aarch64-linux-gnu")
        evidence, = entry["exports"]
        self.assertEqual(evidence["mir_evidence"]["identity_sites"], 1)
        self.assertIn("(rewrite fold-identity (backend mir-archon) (target aarch64-unknown-linux-gnu) (speculative))",
                      evidence["directives"])


if __name__ == "__main__":
    unittest.main()
