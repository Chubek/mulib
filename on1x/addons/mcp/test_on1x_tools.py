#!/usr/bin/env python3
"""Private compiler and snapshot file search tests; --on1xc enables real smoke."""
import argparse
from pathlib import Path
import sys
import tempfile
import unittest

from compiler_sandbox import Sandbox
from core import EvidenceError, Index
from on1x_tools import On1xTools

COMPILER = None
REQUIRE_HIR = False
SOURCE = "fn twice(x:i64)->i64 { return x+x; }\nfn main(){ println(twice(21)); }\n"


class FileSearch(unittest.TestCase):
    def test_filename_and_content_stay_inside_snapshot(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "MANIFEST.yaml").write_text(
                "module_count: 1\nimplemented_module_count: 0\nbounded_module_count: 1\n"
                "modules:\n- name: alpha\n  status: bounded\n  implementation: static-inline\n"
                "  header: kernels/mulib-alpha.h\n  manual: manual/alpha.md\n  dependencies: []\n")
            (root / "kernels").mkdir()
            (root / "manual").mkdir()
            (root / "kernels/mulib-alpha.h").write_text("mention\n" * 55)
            (root / "manual/alpha.md").write_text("mention\n")
            (root / "secret.txt").write_text("mention\n")
            (root / "README.md").write_text("")
            index = Index(root)
            (root / "kernels/mulib-alpha.h").write_text("new contents")
            by_name = index.call("search_files", {"query": "MULIB-ALPHA.H"})
            self.assertEqual(by_name["results"][0]["path"], "kernels/mulib-alpha.h")
            self.assertIn("mention", index.fetch(by_name["results"][0]["id"])["text"])
            by_content = index.call("search_files", {"query": "mention", "mode": "content", "limit": 1})
            self.assertEqual(by_content["matches"], 2)
            self.assertTrue(by_content["truncated"])
            self.assertEqual(index.search_files("alpha.h", "content")["matches"], 1)  # MANIFEST
            self.assertEqual(index.search_files("../secret.txt")["results"], [])
            self.assertEqual(index.search_files("README.md")["results"][0]["id"], None)
            with self.assertRaises(EvidenceError) as error:
                index.call("search_files", {"query": "mention", "mode": "regex"})
            self.assertEqual(error.exception.code, "INVALID_ARGUMENT")


class RecordingSandbox:
    def __init__(self, output=b"(on1x.mir 1 ())\n"):
        self.output = output
        self.runs = []

    def run(self, tool, argv, **request):
        self.runs.append((tool, argv, request))
        return self.output, b""


@unittest.skipUnless(Path("/bin/true").is_file() and sys.platform == "linux", "requires Linux ELF")
class Provider(unittest.TestCase):
    def test_phase_and_profile_are_bounded_and_no_paths_or_flags(self):
        sandbox = RecordingSandbox()
        provider = On1xTools(Path("/bin/true"), sandbox)
        for args in ({"source": SOURCE, "phase": "lir"}, {"source": SOURCE, "phase": ["ast"]},
                     {"source": SOURCE, "phase": "ast", "profile": "debug"},
                     {"source": SOURCE, "phase": "mir", "profile": "--run-vm"},
                     {"source": SOURCE, "phase": "mir", "path": "/etc/passwd"},
                     {"source": "😀" * 2049, "phase": "mir"},
                     {"source": "\0", "phase": "mir"}):
            with self.assertRaises(EvidenceError) as error:
                provider.call("inspect_on1x", args)
            self.assertEqual(error.exception.code, "INVALID_ARGUMENT")
        self.assertEqual(sandbox.runs, [])
        result = provider.call("inspect_on1x", {"source": SOURCE, "phase": "mir", "profile": "debug"})
        self.assertEqual(result["text"], "(on1x.mir 1 ())\n")
        _, argv, input_data = sandbox.runs[0]
        self.assertEqual(argv, ["--emit-ir", "--no-prelude", "--color", "never", "--opt-profile",
                                "debug", "/work/source.on1x"])
        self.assertEqual(input_data, {"source": SOURCE.encode(), "filename": "source.on1x"})

    def test_invalid_and_oversized_output(self):
        provider = On1xTools(Path("/bin/true"), RecordingSandbox(b'{"ast": true}'))
        for output, code in ((b'{"ast": true}', "INVALID_OUTPUT"),
                             (b"(on1x.mir " + b'x' * 22000 + b")", "RESULT_LIMIT")):
            provider.sandbox.output = output
            with self.assertRaises(EvidenceError) as error:
                provider.call("inspect_on1x", {"source": SOURCE, "phase": "mir"})
            self.assertEqual(error.exception.code, code)


class RealCompiler(unittest.TestCase):
    def setUp(self):
        if COMPILER is None:
            self.skipTest("pass --on1xc PATH to run sandboxed native compiler")
        self.provider = On1xTools(COMPILER, Sandbox())

    def test_real_compiler_emission(self):
        for phase, header in (("ast", "(Unit 1 "), ("mir", "(on1x.mir "), ("hir", "(on1x.hir ")):
            with self.subTest(phase=phase):
                try:
                    result = self.provider.call("inspect_on1x", {"source": SOURCE, "phase": phase})
                except EvidenceError as exc:
                    if phase == "hir" and exc.code == "OUTPUT_UNAVAILABLE" and not REQUIRE_HIR:
                        continue
                    raise
                self.assertEqual(result["format"], "sexp")
                self.assertTrue(result["text"].startswith(header))
                self.assertIn("twice", result["text"])
                if phase == "hir":
                    self.assertIn('"i64"', result["text"])
                    self.assertIn("(reference Local)", result["text"])
        c = self.provider.call("compile_on1x", {"source": SOURCE, "profile": "release"})
        self.assertEqual(c["format"], "c11")
        self.assertIn("#include <on1x_runtime.h>", c["text"])
        self.assertIn("twice", c["text"])

    def test_real_compiler_cannot_read_absolute_import_or_enable_ppx(self):
        with tempfile.TemporaryDirectory() as directory:
            other = Path(directory) / "dependency.on1x"
            other.write_text("fn actual() -> i64 { return 42; }\n")
            with self.assertRaises(EvidenceError) as error:
                self.provider.call("compile_on1x", {"source": f'import "{other}";\n' + SOURCE})
            self.assertEqual(error.exception.code, "COMPILER_FAILED")
            self.assertIn("cannot resolve import", str(error.exception))
            with self.assertRaises(EvidenceError) as error:
                self.provider.call("compile_on1x", {"source":
                    '@ppx peeker "/driver/tool";\n@peeker\n' + SOURCE})
            self.assertEqual(error.exception.code, "COMPILER_FAILED")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--on1xc", type=Path)
    parser.add_argument("--expect-hir", action="store_true")
    args, unittest_args = parser.parse_known_args()
    COMPILER = args.on1xc.resolve(strict=True) if args.on1xc else None
    REQUIRE_HIR = args.expect_hir
    unittest.main(argv=[sys.argv[0], *unittest_args])
