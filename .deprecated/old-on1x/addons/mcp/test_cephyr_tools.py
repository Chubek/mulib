"""Unit tests and opt-in smoke coverage with a real Cephyr toolchain."""

import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from cephyr_tools import CephyrTools, FORMATS, MAX_SOURCE, TOOL
from core import EvidenceError


class CephyrValidation(unittest.TestCase):
    def setUp(self):
        # Public tool validation should not need Linux or any native runtime.
        self.provider = object.__new__(CephyrTools)

    def test_descriptor(self):
        descriptor, = self.provider.tools()
        self.assertEqual(descriptor["name"], TOOL)
        schema = descriptor["inputSchema"]
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(set(schema["required"]), {"operation", "source"})
        self.assertEqual(set(schema["properties"]["operation"]["enum"]), set(FORMATS))

    def test_rejects_unknown_options_and_invalid_source_before_spawn(self):
        request = {"operation": "parse_c11", "source": "int value;\n"}
        invalid = [None, [], {}, {**request, "argv": ["--help"]},
                   {**request, "library": "/tmp/override.so"}, {**request, "operation": []},
                   {**request, "operation": "compile"}, {**request, "source": 7},
                   {**request, "source": ""}, {**request, "source": " \n\t"},
                   {**request, "source": "int\0value;"}, {**request, "source": "\ud800"},
                   {**request, "source": "a" * (MAX_SOURCE + 1)},
                   {**request, "source": "é" * (MAX_SOURCE // 2 + 1)}]
        with patch("cephyr_tools.subprocess.Popen") as launch:
            for args in invalid:
                with self.subTest(case=type(args).__name__), self.assertRaises(EvidenceError) as caught:
                    self.provider.call(TOOL, args)
                self.assertEqual(caught.exception.code, "INVALID_ARGUMENT")
            launch.assert_not_called()


@unittest.skipUnless(all(os.environ.get("ON1X_TEST_CEPHYR_" + option)
                         for option in ("BINDINGS", "LIBRARY", "RESOURCES", "ARCHON")),
                     "set ON1X_TEST_CEPHYR_BINDINGS, _LIBRARY, _RESOURCES, and _ARCHON for Linux smoke tests")
class CephyrSmoke(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.args = [os.environ["ON1X_TEST_CEPHYR_" + option]
                    for option in ("BINDINGS", "LIBRARY")]
        cls.kwargs = {
            "resource_root": os.environ["ON1X_TEST_CEPHYR_RESOURCES"],
            "archon_root": os.environ["ON1X_TEST_CEPHYR_ARCHON"],
            "python": os.environ.get("ON1X_TEST_CEPHYR_PYTHON", "/usr/bin/python3"),
        }
        cls.provider = CephyrTools(*cls.args, **cls.kwargs)

    def extract(self, source, operation="parse_c11"):
        return self.provider.call(TOOL, {"operation": operation, "source": source})

    def test_ast_interface_and_mir(self):
        source = "int square(int x) { return x*x; }\n"
        for operation, prefix in (("parse_c11", '(1 (object ("ast"'),
                                  ("emit_interface", "(interface 1"),
                                  ("emit_mir", "archon-mir 1\n")):
            with self.subTest(operation=operation):
                result = self.extract(source, operation)
                self.assertEqual(result["status"], "ok")
                self.assertEqual(result["operation"], operation)
                self.assertEqual(result["format"], FORMATS[operation])
                self.assertTrue(result["output"].startswith(prefix))
                self.assertIn("square", result["output"])
                self.assertEqual(result["diagnostics"], [])
                self.assertFalse(result["diagnostics_truncated"])

    def test_structured_source_diagnostic(self):
        result = self.extract("int =;\n")
        self.assertEqual(result["status"], "compile_error")
        self.assertEqual(result["output"], "")
        error, = result["diagnostics"]
        self.assertEqual(error["severity"], "error")
        self.assertEqual(error["code"], "cephyr.parse")
        self.assertEqual(error["line"], 1)
        self.assertEqual(error["column"], 5)
        self.assertIn("expected", error["message"])

    def test_host_file_and_python_environment_are_not_visible(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "sentinel.h"
            path.write_text("int PRIVATE_TEST_SENTINEL;\n")
            # The worker script is present inside the sandbox but is not a
            # Cephyr TOML config. Reading it as one would fail extraction.
            with patch.dict(os.environ, {"CEPHYR_CONFIG": "/app/worker.py",
                                         "CEPHYR_LIBRARY": str(path), "PYTHONPATH": folder}):
                result = self.extract(f'#include "{path}"\n')
                self.assertEqual(self.extract("int value;\n")["status"], "ok")
        self.assertEqual(result["status"], "compile_error")
        self.assertIn("not found", result["diagnostics"][0]["message"])
        self.assertNotIn("PRIVATE_TEST_SENTINEL", str(result))

    def test_disabled_sandbox_does_not_fall_back(self):
        provider = CephyrTools(*self.args, **self.kwargs, bwrap="/usr/bin/false")
        with self.assertRaises(EvidenceError) as caught:
            provider.call(TOOL, {"operation": "parse_c11", "source": "int value;\n"})
        self.assertEqual(caught.exception.code, "CEPHYR_FAILED")

    def test_large_output_is_rejected_instead_of_partially_returned(self):
        source = "".join(f"int variable{i};\n" for i in range(200))
        with self.assertRaises(EvidenceError) as caught:
            self.extract(source)
        self.assertEqual(caught.exception.code, "RESULT_LIMIT")


if __name__ == "__main__":
    unittest.main()
