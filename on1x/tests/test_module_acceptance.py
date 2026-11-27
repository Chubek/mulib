"""Public entry-point acceptance for declared modules alongside legacy imports."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ON1X = Path(__file__).resolve().parents[1]
OPTIONS = None


class Modules(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix="project-", dir=OPTIONS.work_dir)
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        (self.root / "tmp").mkdir()

    def write(self, filename, text):
        path = self.root / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def command(self, *args, error=None):
        args = [str(arg) for arg in args]
        result = subprocess.run(args, cwd=self.root, capture_output=True, text=True, timeout=90,
                                env={**os.environ, "TMPDIR": str(self.root / "tmp")})
        if error is None:
            self.assertEqual(result.returncode, 0, (args, result.stderr))
        else:
            self.assertNotEqual(result.returncode, 0, args)
            self.assertEqual(result.stdout, "")
            self.assertRegex(result.stderr, re.compile(error, re.IGNORECASE))
        return result.stdout

    def compiler(self, *args, error=None):
        return self.command(OPTIONS.on1xc, "--stdlib", ON1X / "stdlib", *args, error=error)

    def project(self, output, *, error=None):
        return self.command(
            sys.executable, ON1X / "tools/on1x-project.py", "build", self.root,
            "--on1xc", OPTIONS.on1xc, "--cc", OPTIONS.cc, "--stdlib", ON1X / "stdlib",
            "--backend", "c11", "--output-dir", output, error=error,
        )

    def emit_and_run(self, entry):
        out = self.root / "generated.c"
        exe = self.root / "generated"
        self.compiler("--emit-c", entry, "-o", out)
        self.command(OPTIONS.cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                     "-I", ON1X / "runtime", out, ON1X / "runtime/on1x_runtime.c", "-pthread", "-o", exe)
        return self.command(exe)

    def application(self, rows):
        self.write("On1xfile", 'on1xfile 1\nkind executable\nname "app"\ndirectory "src"\n'
                   'target "app" "src/main.on1x"\n' + rows)
        self.write("src/On1xfile.m", 'on1xfile 1\nsource "main.on1x"\n')

    def test_side_by_side_resolution_and_selector_alias(self):
        self.write("A/Answer.on1xm", """
module A.Answer;
public fn answer() -> i64 { return helper(); }
private fn helper() -> i64 { return 40; }
""")
        self.write("legacy.on1x", "fn two() -> i64 { return 2; }\n")
        entry = self.write("main.on1x", """
import A.Answer as A1;
import A.Answer : answer as get;
import "legacy.on1x";
fn program() -> i64 { return 2; }
fn main() { println(A1.answer() + get() + two() + program()); }
""")
        self.assertEqual(self.emit_and_run(entry), "84\n")
        self.write("main.on1x", """
from A.Answer import (helper as get);
fn main() { println(get()); }
""")
        out = self.write("preserved.c", "keep me\n")
        self.compiler("--emit-c", entry, "-o", out,
                      error=r"helper.*(?:private|export|visible)|(?:private|export|visible).*helper")
        self.assertEqual(out.read_text(encoding="utf-8"), "keep me\n")

    def test_source_kind_and_declared_name_errors(self):
        module = self.write("A/Answer.on1xm", "fn answer() -> i64 { return 42; }\n")
        self.compiler("--check", module, error=r"module.*(?:header|declaration)|expected.*module")
        source = self.write("wrong.on1x", "module A.Answer;\nfn main() {}\n")
        self.compiler("--check", source, error=r"module.*(?:header|declaration|\.on1xm)")
        self.write("A/Answer.on1xm", "module A.Other;\npublic fn answer() -> i64 { return 42; }\n")
        source = self.write("entry.on1x", "import A.Answer;\nfn main() {}\n")
        self.compiler("--check", source, error=r"A\.Answer|A\.Other")

    def test_explicit_program_main_is_not_exported_or_run(self):
        self.write("tool.on1x", """
fn answer() -> i64 { return 42; }
fn main() { println(99); }
""")
        entry = self.write("client.on1x", """
import program "tool.on1x" as tool;
fn main() { println(tool.answer()); }
""")
        self.assertEqual(self.emit_and_run(entry), "42\n")
        self.write("client.on1x", """
import program "tool.on1x" : answer as get;
fn main() { println(get()); }
""")
        self.assertEqual(self.emit_and_run(entry), "42\n")
        self.write("client.on1x", """
import program "tool.on1x" : main as run_tool;
fn main() { run_tool(); }
""")
        self.compiler("--check", entry, error=r"(?:export|entry point).*main|main.*(?:export|entry point)")
        self.write("client.on1x", """
import program "tool.on1x" as tool;
fn main() { tool.main(); }
""")
        self.compiler("--check", entry, error=r"(?:export|entry point).*main|main.*(?:export|entry point)")

    def test_explicit_program_disables_transitive_source_ppx(self):
        self.write("leaf.on1x", '@ppx identity "must-not-run.py";\nfn answer() -> i64 { return 42; }\n')
        self.write("tool.on1x", """
import leaf as leaf;
fn answer() -> i64 { return leaf.answer(); }
fn main() {}
""")
        entry = self.write("client.on1x", """
import program tool as tool;
fn main() { println(tool.answer()); }
""")
        out = self.write("preserved.c", "keep me\n")
        self.compiler("--allow-source-ppx", "--emit-c", entry, "-o", out,
                      error=r"program.*(?:ppx|capabilit)|(?:ppx|capabilit).*program")
        self.assertEqual(out.read_text(encoding="utf-8"), "keep me\n")

    def test_manifest_fragments_graph_and_atomic_publish(self):
        self.application("""module "A.Wrapper" "src/api.on1xm"
module "A.Wrapper" "src/impl.on1xm"
module "A.Values" "src/value.on1xm"
""")
        self.write("src/api.on1xm", """
module A.Wrapper;
import A.Values as V;
public fn answer() -> i64 { return V.value() + 2; }
""")
        self.write("src/impl.on1xm", """
module A.Wrapper;
import A.Values as V;
public fn another() -> i64 { return V.value() + 2; }
""")
        self.write("src/value.on1xm", """
module A.Values;
shared elevate(2) fn value() -> i64 { return 40; }
""")
        self.write("src/main.on1x", """
from A.Wrapper import (answer, another);
fn main() { println(answer()); println(another()); }
""")
        output = self.root / "output"
        self.project(output)
        self.assertEqual(self.command(output / "app"), "42\n42\n")
        marker = output / ".on1x-build.json"
        before = marker.read_bytes()
        graph = json.loads(before)["module_graph"]
        self.assertEqual(graph, [
            {"package": "app", "name": "A.Values", "dependencies": []},
            {"package": "app", "name": "A.Wrapper",
             "dependencies": [{"package": "app", "name": "A.Values"}]},
        ])
        self.write("src/impl.on1xm", "module A.Wrong;\npublic fn another() -> i64 { return 42; }\n")
        self.project(output, error=r"A\.Wrong|does not match|expected.+A\.Wrapper")
        self.assertEqual(marker.read_bytes(), before)
        self.assertEqual(self.command(output / "app"), "42\n42\n")

    def test_manifest_signature_seals_the_actual_build(self):
        self.application("""module "A.API" "src/provider.on1xm"
module-interface "A.API" "src/api.on1xi"
module-interface-policy "A.API" required
""")
        self.write("src/provider.on1xm", """
module A.API;
public fn answer() -> i64 { return 42; }
public fn hidden() -> i64 { return 99; }
""")
        self.write("src/api.on1xi", "abstract module A.API;\nfn answer() -> i64;\n")
        self.write("src/main.on1x", "import A.API as API;\nfn main() { println(API.answer()); }\n")
        output = self.root / "output"
        self.project(output)
        self.assertEqual(self.command(output / "app"), "42\n")
        marker = output / ".on1x-build.json"
        before = marker.read_bytes()

        self.write("src/main.on1x", "import A.API as API;\nfn main() { println(API.hidden()); }\n")
        self.project(output, error=r"hidden.*(?:signature|export|visible)|(?:signature|export|visible).*hidden")
        self.assertEqual(marker.read_bytes(), before)
        self.assertEqual(self.command(output / "app"), "42\n")

        self.write("src/main.on1x", "import A.API as API;\nfn main() { println(API.answer()); }\n")
        self.write("src/api.on1xi", "abstract module A.API;\nfn answer() -> bool;\n")
        self.project(output, error=r"(?:signature|interface).*(?:mismatch|expected)")
        self.assertEqual(marker.read_bytes(), before)


def main():
    global OPTIONS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--on1xc", type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--work-dir", type=lambda value: Path(value).resolve(), required=True)
    OPTIONS, rest = parser.parse_known_args()
    OPTIONS.work_dir.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[sys.argv[0], *rest])


if __name__ == "__main__":
    main()
