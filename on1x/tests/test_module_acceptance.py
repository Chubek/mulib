"""Public entry-point acceptance for declared modules alongside legacy imports."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
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

    def command(self, *args, error=None, quiet=False):
        args = [str(arg) for arg in args]
        result = subprocess.run(args, cwd=self.root, capture_output=True, text=True, timeout=90,
                                env={**os.environ, "TMPDIR": str(self.root / "tmp")})
        if error is None:
            self.assertEqual(result.returncode, 0, (args, result.stderr))
            if quiet:
                self.assertEqual(result.stderr, "", args)
        else:
            self.assertNotEqual(result.returncode, 0, args)
            self.assertEqual(result.stdout, "")
            self.assertRegex(result.stderr, re.compile(error, re.IGNORECASE))
        return result.stdout

    def compiler(self, *args, error=None):
        return self.command(OPTIONS.on1xc, "--stdlib", ON1X / "stdlib", *args, error=error)

    def project(self, output, *, error=None, backend="c11", root=None):
        return self.command(
            sys.executable, ON1X / "tools/on1x-project.py", "build", root or self.root,
            "--on1xc", OPTIONS.on1xc, "--cc", OPTIONS.cc, "--stdlib", ON1X / "stdlib",
            "--backend", backend, "--output-dir", output, error=error,
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

    def test_dotted_receivers_do_not_bypass_named_module_scope(self):
        self.write("A/Choice.on1xm", """
module A.Choice;
public enum Option { Ready }
private enum Hidden { Never }
private fn hidden() -> i64 { return 99; }
""")
        entry = self.write("main.on1x", """
import A.Choice as C;
fn main() {
  let option = C.Option::Ready;
  println(match option with { Ready() => 42 });
  let C = record{answer: 43}; println(C.answer);
}
""")
        self.assertEqual(self.emit_and_run(entry), "42\n43\n")
        cases = (
            ("println(C.hidden.field);", r"hidden.*(?:private|visible)|(?:private|visible).*hidden"),
            ("let x = C.Hidden::Never;", r"Hidden.*(?:private|visible)|(?:private|visible).*Hidden"),
        )
        for statement, diagnostic in cases:
            with self.subTest(statement=statement):
                self.write("main.on1x", f"import A.Choice as C;\nfn main() {{ {statement} }}\n")
                self.compiler("--check", entry, error=diagnostic)

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
fn main() { println(tool.answer()); }
""")
        self.compiler("--check", entry, error=r"unknown (?:module|function|name)|not imported|unbound")
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

    def test_unit_v1_ppx_cannot_transform_program_import_graph(self):
        self.write("tool.on1x", "fn answer() -> i64 { return 42; }\nfn main() {}\n")
        entry = self.write("client.on1x", """
import program tool : answer;
fn main() { println(answer()); }
""")
        out = self.write("preserved.c", "keep me\n")
        self.compiler("--ppx", "denied", "/bin/false", "--emit-c", entry, "-o", out,
                      error=r"ppx.*(?:module|scope|admission)|(?:module|scope).*ppx")
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
        value_source = """
module A.Values;
shared elevate(2) fn value() -> i64 { return 40; }
"""
        self.write("src/value.on1xm", value_source)
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
        self.write("src/value.on1xm", value_source.replace("elevate(2)", "elevate(1)"))
        self.project(output, error=r"value.*(?:shared|scope|visible)|(?:shared|scope|visible).*value")
        self.assertEqual(marker.read_bytes(), before)
        self.write("src/value.on1xm", value_source)

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

        self.write("src/main.on1x", "import A.API as API;\nfn main() { println(API.hidden.field); }\n")
        self.project(output, error=r"hidden.*(?:signature|export|visible)|(?:signature|export|visible).*hidden")
        self.assertEqual(marker.read_bytes(), before)

        self.write("src/main.on1x", "import A.API as API;\nfn main() { println(API.answer()); }\n")
        self.write("src/api.on1xi", "abstract module A.API;\nfn answer() -> bool;\n")
        self.project(output, error=r"(?:signature|interface).*(?:mismatch|expected)")
        self.assertEqual(marker.read_bytes(), before)

    def test_manifest_program_requires_explicit_admission(self):
        self.application('program "tool" "src/tool.on1x"\n')
        roles = 'on1xfile 1\nsource "main.on1x"\nlibrary "tool.on1x"\n'
        self.write("src/On1xfile.m", roles)
        self.write("src/tool.on1x", "fn answer() -> i64 { return 42; }\nfn main() { println(99); }\n")
        self.write("src/main.on1x", "import program tool : answer;\nfn main() { println(answer()); }\n")
        output = self.root / "output"
        self.project(output)
        self.assertEqual(self.command(output / "app"), "42\n")
        marker = output / ".on1x-build.json"
        before = marker.read_bytes()

        self.application("")
        self.write("src/On1xfile.m", roles)
        self.project(output, error=r"program.*(?:not.*(?:declared|index|admit)|admission|opt.in)")
        self.assertEqual(marker.read_bytes(), before)
        self.assertEqual(self.command(output / "app"), "42\n")

    def test_opaque_provider_identity_and_project_atomicity(self):
        self.application("""module "A.Vault" "src/vault.on1xm"
module "A.Other" "src/other.on1xm"
module-interface "A.Vault" "src/vault.on1xi"
module-interface "A.Other" "src/other.on1xi"
module-interface-policy "A.Vault" required
module-interface-policy "A.Other" required
""")
        provider = """module A.Vault;
public record Token { secret: i64 }
public enum Choice { Empty, Full(i64) }
public fn issue(n:i64) -> Token { return record<Token>{secret:n}; }
public fn redeem(token:Token) -> i64 { return token.secret; }
public fn wrap(n:i64) -> Choice { return Choice::Full(n); }
public fn unwrap(value:Choice) -> i64 {
    return match value with { Empty() => 0, Full(n) => n };
}
"""
        signature = """abstract module A.Vault;
type Token;
type Choice;
fn issue(n:i64) -> Token;
fn redeem(token:Token) -> i64;
fn wrap(n:i64) -> Choice;
fn unwrap(value:Choice) -> i64;
"""
        for name, declared in (("vault", "Vault"), ("other", "Other")):
            self.write(f"src/{name}.on1xm", provider.replace("A.Vault", f"A.{declared}"))
            self.write(f"src/{name}.on1xi", signature.replace("A.Vault", f"A.{declared}"))

        imports = "import A.Vault as V; import A.Other as O;\n"
        good = imports + """
fn pass(token:V.Token) -> V.Token { return token; }
fn main() { let t = pass(V.issue(20)); println(V.redeem(t) + O.unwrap(O.wrap(22))); }
"""
        entry = self.write("src/main.on1x", good)
        output = self.root / "output"
        self.project(output)
        executable = output / "app"
        marker = output / ".on1x-build.json"
        previous = marker.read_bytes(), executable.read_bytes()
        self.assertEqual(self.command(executable, quiet=True), "42\n")

        opaque = r"opaque module type representation is not accessible"
        cases = {
            "dot projection": ("let v = V.issue(1); println(v.secret);", opaque),
            "index projection": ('let v = V.issue(1); println(v["secret"]);', opaque),
            "record construction": ("let v = record<V.Token>{secret: 1};", opaque),
            "record destructuring": (
                "let v = V.issue(1); println(match v with { record{secret:n} => n });",
                opaque,
            ),
            "enum construction": ("let v = V.Choice::Full(1);", opaque),
            "enum destructuring": (
                "let v = V.wrap(1); println(match v with { Empty() => 0, Full(n) => n });",
                opaque,
            ),
            "layout equivalence": ("println(O.redeem(V.issue(1)));", r"nominal|expected|incompatib"),
        }
        for name, (body, diagnostic) in cases.items():
            with self.subTest(case=name):
                entry.write_text(imports + f"fn main() {{ {body} }}\n", encoding="utf-8")
                self.project(output, error=diagnostic)
                self.assertEqual((marker.read_bytes(), executable.read_bytes()), previous)

        entry.write_text(good, encoding="utf-8")
        for backend in ("mir-c11", "mir-vm"):
            with self.subTest(backend=backend):
                self.project(output, backend=backend, error=r"sealed module value ABI.*direct C11")
                self.assertEqual((marker.read_bytes(), executable.read_bytes()), previous)

    def test_energy_budget_legacy_and_sealed_portable_backends(self):
        example = ON1X / "examples/energy-budget"
        expected = (example / "expected.txt").read_text(encoding="utf-8")
        for variant in ("legacy", "sealed"):
            with self.subTest(variant=variant):
                root = self.root / variant
                shutil.copytree(example, root)
                entry = root / "src/main.on1x"
                if variant == "sealed":
                    legacy_import = 'import "schedule.on1x";'
                    main = entry.read_text(encoding="utf-8")
                    self.assertIn(legacy_import, main)
                    entry.write_text(main.replace(legacy_import, "import demo.energy : duty_minutes;"),
                                     encoding="utf-8")
                    schedule = root / "src/schedule.on1x"
                    body = schedule.read_text(encoding="utf-8")
                    self.assertIn("fn duty_minutes(", body)
                    schedule.unlink()
                    self.write(f"{variant}/src/demo/energy.on1xm", "module demo.energy;\n" +
                               body.replace("fn duty_minutes(", "public fn duty_minutes(", 1))
                    self.write(f"{variant}/src/demo/energy.on1xi", "abstract module demo.energy;\n"
                               "fn duty_minutes(day: i64) -> (i64, i64, i64);\n")
                    self.write(f"{variant}/src/On1xfile.m", 'on1xfile 1\nsource "main.on1x"\n')
                    manifest = root / "On1xfile"
                    self.write(f"{variant}/On1xfile", manifest.read_text(encoding="utf-8") +
                               'module "demo.energy" "src/demo/energy.on1xm"\n'
                               'module-interface "demo.energy" "src/demo/energy.on1xi"\n'
                               'module-interface-policy "demo.energy" required\n')
                for backend in ("c11", "mir-c11", "mir-vm"):
                    with self.subTest(backend=backend):
                        output = self.root / f"{variant}-{backend}"
                        self.project(output, backend=backend, root=root)
                        self.assertEqual(self.command(output / "energy-budget", quiet=True), expected)
                        if backend == "mir-vm":
                            self.command(OPTIONS.on1xc.with_name("on1x-vm"), "--verify",
                                         output / "energy-budget.o1vm")
                saved = self.root / f"{variant}.mir"
                self.compiler("--emit-ir", entry, "-o", saved)
                self.assertEqual(self.command(OPTIONS.on1xc, "--ir-input", "--run-ir", saved, quiet=True),
                                 expected)


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
