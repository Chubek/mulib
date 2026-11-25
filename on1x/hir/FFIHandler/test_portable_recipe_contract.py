"""Validate the portable recipe companion without invoking Cephyr."""

import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from portable_recipe_contract import CONTRACTS_DIR, load_portable_recipe_contract


class PortableRecipeContract(unittest.TestCase):
    def setUp(self):
        self.fixture = CONTRACTS_DIR / "clamp.portable-recipe.json"
        self.document = json.loads(self.fixture.read_text(encoding="utf-8"))
        self.symbol = "mulib_clamp_i64"
        self.export = self.document["functions"][self.symbol]
        self.link = {
            "kernel": "clamp",
            "revision": self.document["revision"],
            "arities": {
                self.symbol: 3, "mulib_clamp_add_i64": 2, "mulib_clamp_sub_i64": 2,
            },
        }

    def load(self, text=None):
        with TemporaryDirectory() as temporary:
            path = Path(temporary) / self.fixture.name
            path.write_text(json.dumps(self.document) if text is None else text, encoding="utf-8")
            return load_portable_recipe_contract(path, **self.link)

    def test_loads_versioned_clamp_fixture(self):
        result = load_portable_recipe_contract(self.fixture, **self.link)
        self.assertEqual(result[self.symbol].guard, ("le", "$1", "$2"))
        self.assertEqual(result["mulib_clamp_add_i64"].guard, "true")
        self.assertEqual(result["mulib_clamp_sub_i64"].guard, "true")
        self.assertEqual([request.render() for request in result[self.symbol].directives], [
            "(hint kernel-hot (target x86_64-unknown-linux-gnu) (speculative))",
            "(hint private-code (backend mir-c11))",
            "(rewrite fold-identity (speculative))",
        ])

    def test_can_omit_directive_requests(self):
        del self.export["directives"]
        self.assertEqual(self.load()[self.symbol].directives, ())

    def test_rejects_companion_link_mismatches(self):
        revision = self.document["revision"]
        self.document["revision"] = "sha256:" + "0" * 64
        with self.assertRaisesRegex(ValueError, "revision does not match"):
            self.load()
        self.document["revision"] = revision
        functions = self.document["functions"]
        del functions[self.symbol]
        with self.assertRaisesRegex(ValueError, "missing portable companion export"):
            self.load()
        functions[self.symbol] = self.export
        functions["mulib_not_reviewed"] = {"guard": "true"}
        with self.assertRaisesRegex(ValueError, "unexpected portable companion export"):
            self.load()

    def test_rejects_missing_or_ill_typed_guard(self):
        del self.export["guard"]
        with self.assertRaisesRegex(ValueError, "missing fields: guard"):
            self.load()
        for guard in (True, ["le", "$1", "$3"], ["le", "$1", "$00"], ["ge", "$1", 1.0],
                      ["and", "true", "$0"], ["le", "$1", 1 << 63], ["lt", "$1"]):
            with self.subTest(guard=guard), self.assertRaisesRegex(ValueError, "guard"):
                self.export["guard"] = guard
                self.load()
        guard = "true"
        for _ in range(33):
            guard = ["and", "true", guard]
        self.export["guard"] = guard
        with self.assertRaisesRegex(ValueError, "depth or node limit"):
            self.load()

    def test_rejects_ambiguous_or_unknown_fields(self):
        text = json.dumps(self.document).replace('"guard":', '"guard": "true", "guard":', 1)
        with self.assertRaisesRegex(ValueError, "duplicate JSON field 'guard'"):
            self.load(text)
        self.export["body"] = "$0"
        with self.assertRaisesRegex(ValueError, "unknown fields: body"):
            self.load()

    def test_validates_explicit_directive_scope_and_options(self):
        self.export["directives"] = [
            {"kind": "hint", "action": "no-inline", "backend": "mir-c11", "speculative": True},
        ]
        self.assertEqual(self.load()[self.symbol].directives[0].render(),
                         "(hint no-inline (backend mir-c11) (speculative))")
        for request, message in (
            ({"kind": "hint", "action": "fast-math"}, "unsupported hint directive"),
            ({"kind": "hint", "action": "no-inline", "backend": "*"}, "requires effective backend mir-c11"),
            ({"kind": "hint", "action": "cold-code", "target": "a) (guard true"}, "target selector"),
            ({"kind": "rewrite", "action": "fold-identity", "speculative": 1}, "must be a JSON boolean"),
        ):
            with self.subTest(request=request), self.assertRaisesRegex(ValueError, message):
                self.export["directives"] = [request]
                self.load()

    def test_rejects_duplicate_effective_requests(self):
        self.export["directives"] = [
            {"kind": "rewrite", "action": "fold-identity"},
            {"kind": "rewrite", "action": "fold-identity", "backend": "*", "speculative": False},
        ]
        with self.assertRaisesRegex(ValueError, "duplicate effective directive request"):
            self.load()


if __name__ == "__main__":
    unittest.main()
