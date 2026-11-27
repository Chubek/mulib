#!/usr/bin/env python3
"""Native publication guards: real frontend and ELF tools, deterministic backend fixture.

This does not qualify Archon lowering. The fixture substitutes only its assembly
generation, so the test needs no unbuilt Archon target or deleted stdlib sources.
"""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
COMPILER = Path(sys.argv.pop(1)).resolve()
BUILD = Path(sys.argv.pop(1)).resolve()
HELPER = Path(os.environ.get("ON1X_TEST_NATIVE_HELPER", ROOT / "tools/on1x-native-build.py"))
STDLIB = subprocess.check_output([COMPILER, "--print-stdlib"], text=True).strip()
BUILD.mkdir(parents=True, exist_ok=True)
SPEC = importlib.util.spec_from_file_location("native_build", HELPER)
NATIVE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NATIVE)
ASSEMBLY = ('.text\n.globl main\n.type main,@function\nmain:\n'
            ' movl $42,%eax\n ret\n.section .note.GNU-stack,"",@progbits\n')


class NativeInputGuard(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="native-inputs-", dir=BUILD)
        self.addCleanup(self.temp.cleanup)
        # Deliberately include spaces and Unicode in all paths in the graph.
        self.root = Path(self.temp.name) / "project λ with spaces"
        self.root.mkdir()
        self.entry = self.root / "app.on1x"
        self.first = self.root / "provider.on1xm"
        self.second = self.root / "provider-extra.on1xm"
        self.interface = self.root / "provider.on1xi"
        self.index = self.root / "modules.json"
        self.output = self.root / "program"
        self.marker = self.root / "assembly-started"
        self.entry.write_text("import Test.Provider as P; fn main()->i64{return P.answer();}")
        self.first.write_text("module Test.Provider; public fn answer()->i64{return helper();}")
        self.second.write_text("module Test.Provider; fn helper()->i64{return 42;}")
        self.interface.write_text("abstract module Test.Provider; fn answer()->i64;")
        self.index.write_text(json.dumps({
            "schema": "on1x.project-modules/v1", "project": str(self.root),
            "programs": {}, "members": {"Test.Provider": {
                "package": str(self.root), "declared_name": "Test.Provider",
                "fragments": [str(self.first), str(self.second)],
                "interfaces": [str(self.interface)],
            }},
        }))
        self.backend = self.root / "backend"
        self.backend.write_text(
            "#!/usr/bin/python3\nimport pathlib,sys\nargs=sys.argv[1:]\n"
            "if args == ['--print-target']: print('x86_64-unknown-linux-gnu')\n"
            "elif args == ['--print-backend-target']: print('AMD64/SysVAMD64')\n"
            "else:\n"
            " assert pathlib.Path(args[args.index('--ir-input')+1]).read_bytes()\n"
            f" pathlib.Path({str(self.marker)!r}).write_text('started')\n"
            f" pathlib.Path(args[args.index('-o')+1]).write_text({ASSEMBLY!r})\n"
        )
        self.backend.chmod(0o700)
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps(dict(
            target="AMD64", abi="SysVAMD64", backend_abi_version=1,
            machineir_schema_version=1)))
        self.scalar = self.root / "libscalar.a"
        self.runtime = self.root / "libruntime.a"
        for archive in (self.scalar, self.runtime):
            subprocess.run([shutil.which("ar"), "rcs", archive], check=True, timeout=10)
        self.inputs = [self.entry, self.first, self.second, self.interface, self.index]
        self.originals = {path: path.read_bytes() for path in self.inputs}

    def run_build(self, destination=None, extra=(), *, forwarded=None):
        if forwarded is None:
            forwarded = ["--no-prelude", "--stdlib", STDLIB,
                         "--project-module-index", str(self.index)]
        command = [sys.executable, HELPER, self.entry, "-o", destination or self.output,
                   "--target", "x86_64-unknown-linux-gnu", "--cc", shutil.which("cc"),
                   "--on1xc", COMPILER, "--archon-mir", self.backend,
                   "--scalar-abi", self.scalar, "--runtime", self.runtime,
                   "--archon-manifest", self.manifest, *extra]
        command += ["--on1xc-arg=" + value for value in forwarded]
        environment = os.environ.copy()
        environment["PYTHONPATH"] = str(ROOT / "tools")
        result = subprocess.run(command, text=True, capture_output=True,
                                timeout=40, env=environment)
        self.assertFalse(list(self.root.glob(".on1x-native-*")), result.stderr)
        return result

    def assert_inputs_unchanged(self):
        self.assertEqual(self.originals, {path: path.read_bytes() for path in self.inputs})

    def test_real_elf_publication_and_emit_only(self):
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.output.read_bytes().startswith(b"\x7fELF"))
        self.assertEqual(subprocess.run([self.output], timeout=5).returncode, 42)
        result = self.run_build(extra=["--emit-only"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_text(), ASSEMBLY)
        self.assert_inputs_unchanged()

    def test_all_fragment_signature_and_index_destinations_reject(self):
        for destination in self.inputs:
            with self.subTest(destination=destination.name):
                try:
                    result = self.run_build(destination, ["--emit-only"])
                    self.assertNotEqual(result.returncode, 0, result.stderr)
                    self.assertIn("output aliases", result.stderr)
                    self.assertFalse(self.marker.exists())
                    self.assert_inputs_unchanged()
                finally:
                    # Isolate subcases even against a broken baseline helper.
                    for path, data in self.originals.items():
                        path.write_bytes(data)
                    self.marker.unlink(missing_ok=True)

    def test_symlink_alias_rejects_without_replacing_link(self):
        destination = self.root / "alias"
        for target in (self.second, self.interface, self.index):
            with self.subTest(target=target.name):
                destination.symlink_to(target.name)
                try:
                    result = self.run_build(destination, ["--emit-only"])
                    self.assertNotEqual(result.returncode, 0, result.stderr)
                    self.assertIn("output aliases", result.stderr)
                    self.assertTrue(destination.is_symlink())
                    self.assertEqual(destination.readlink(), Path(target.name))
                    self.assert_inputs_unchanged()
                finally:
                    destination.unlink(missing_ok=True)
                    self.marker.unlink(missing_ok=True)

    def test_executable_destination_cannot_replace_signature(self):
        result = self.run_build(self.interface)
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertIn("output aliases", result.stderr)
        self.assertFalse(self.marker.exists())
        self.assert_inputs_unchanged()

    def test_frontend_rejection_preserves_successful_output(self):
        result = self.run_build(extra=["--emit-only"])
        self.assertEqual(result.returncode, 0, result.stderr)
        previous = self.output.read_bytes()
        self.marker.unlink()
        self.second.write_text("module Test.Provider; fn helper()->i64{return unknown_value;}")
        result = self.run_build(extra=["--emit-only"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown_value", result.stderr)
        self.assertFalse(self.marker.exists())
        self.assertEqual(self.output.read_bytes(), previous)

    def test_default_library_discovery_without_project_index(self):
        self.entry.write_text('import "legacy.on1x"; fn main()->i64{return answer();}')
        provider = self.root / "legacy.on1x"
        provider.write_text("fn answer()->i64{return 42;}")
        before = provider.read_bytes()
        result = self.run_build(provider, ["--emit-only"], forwarded=["--no-prelude"])
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertIn("output aliases", result.stderr)
        self.assertEqual(provider.read_bytes(), before)
        result = self.run_build(extra=["--emit-only"], forwarded=["--no-prelude"])
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_inspection_does_not_launch_ppx(self):
        # A registered transformer must not run in the observational preflight,
        # including when the destination is rejected before ordinary compilation.
        ppx = self.root / "ppx"
        marker = self.root / "ppx-started"
        ppx.write_text(f"#!/bin/sh\ntouch '{marker}'\nexit 91\n")
        ppx.chmod(0o700)
        forwarded = ["--no-prelude", "--project-module-index", str(self.index),
                     "--ppx", "Test", str(ppx), "--allow-source-ppx"]
        result = self.run_build(self.second, ["--emit-only"], forwarded=forwarded)
        self.assertNotEqual(result.returncode, 0, result.stderr)
        self.assertIn("output aliases", result.stderr)
        self.assertFalse(marker.exists())
        self.assert_inputs_unchanged()

    def test_ir_input_retains_existing_profile(self):
        mir = self.root / "source.mir"
        subprocess.run([COMPILER, "--no-prelude", "--emit-ir", self.entry,
                        "--project-module-index", self.index, "-o", mir],
                       check=True, capture_output=True, timeout=20)
        self.entry = mir
        result = self.run_build(extra=["--ir-input", "--emit-only"], forwarded=[])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_text(), ASSEMBLY)


class SourceInputProtocol(unittest.TestCase):
    def test_default_library_query_uses_the_same_ordered_search_flags(self):
        with tempfile.TemporaryDirectory(dir=BUILD) as temporary:
            stage = Path(temporary)
            source = stage / "entry.on1x"
            source.write_text("fn main(){}")

            class Runner:
                def __init__(self):
                    self.commands = []

                def run(self, command, **options):
                    self.commands.append(command)
                    if "--print-stdlib" in command:
                        return str(stage / "library-need-not-exist-with-no-prelude")
                    (stage / "source-inputs.json").write_text(json.dumps(dict(
                        schema="on1x.project-module-graph/v2", sources=[str(source)],
                        auxiliary=[])))

            runner = Runner()
            flags = ["--no-prelude", "--stdlib-path", "first", "-I", "second"]
            NATIVE.source_inputs(runner, COMPILER, source, flags, stage)
            self.assertEqual(runner.commands[0], [COMPILER, "--print-stdlib", *flags])
            self.assertEqual(runner.commands[1][5:-2], flags)
            self.assertEqual(runner.commands[1][-2:], [
                "--stdlib", str(stage / "library-need-not-exist-with-no-prelude")])

    def test_malformed_and_incomplete_observations_fail_closed(self):
        with tempfile.TemporaryDirectory(dir=BUILD) as temporary:
            stage = Path(temporary)
            source = stage / "entry.on1x"
            source.write_text("fn main(){}")
            valid = dict(schema="on1x.project-module-graph/v2",
                         sources=[str(source)], auxiliary=[])

            class Runner:
                def __init__(self, report):
                    self.report = report

                def run(self, command, **options):
                    self.command = command
                    (stage / "source-inputs.json").write_text(json.dumps(self.report))

            for report in ([], {}, dict(valid, schema="on1x.project-module-graph/v99"),
                           dict(valid, sources=[]), dict(valid, sources="not an array"),
                           dict(valid, auxiliary=["relative.on1xi"]),
                           dict(valid, auxiliary=[123]),
                           dict(valid, auxiliary=[str(stage / "missing.on1xi")]),
                           dict(valid, auxiliary=["x"] * 16385)):
                with self.subTest(report=str(report)[:160]):
                    with self.assertRaises((NATIVE.BuildError, OSError)):
                        NATIVE.source_inputs(Runner(report), COMPILER, source,
                                             ["--stdlib", STDLIB], stage)

    def test_observation_filters_transformers_but_keeps_resolution_order(self):
        with tempfile.TemporaryDirectory(dir=BUILD) as temporary:
            stage = Path(temporary)
            source, index, schema, catalog = [stage / name for name in
                                             ("entry.on1x", "index.json", "schema", "catalog")]
            for path in (source, index, schema, catalog):
                path.write_text("input")

            class Runner:
                def run(self, command, **options):
                    self.command = command
                    (stage / "source-inputs.json").write_text(json.dumps(dict(
                        schema="on1x.project-module-graph/v2", sources=[str(source)],
                        auxiliary=[])))

            runner = Runner()
            forwarded = ["-I", "one", "--ppx", "P", "never-execute",
                         "--module-path", "two", "--project-module-index", str(index),
                         "--ppx-schema", "P", str(schema), "--diagnostic-catalog", str(catalog),
                         "--allow-source-ppx", "--memory-check"]
            protected = NATIVE.source_inputs(runner, COMPILER, source, forwarded, stage)
            self.assertEqual(protected, {source, index, schema, catalog})
            self.assertEqual(runner.command[5:], ["-I", "one", "--module-path", "two",
                                                  "--project-module-index", str(index)])


if __name__ == "__main__":
    unittest.main()
