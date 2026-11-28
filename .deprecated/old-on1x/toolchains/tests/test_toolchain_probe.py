"""Real GNU-host toolchain probes plus narrowly scoped failure injection."""

import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "on1x-toolchain-probe.py"
TARGET = "x86_64-unknown-linux-gnu"


@unittest.skipUnless(sys.platform == "linux" and platform.machine() == "x86_64",
                     "requires a native AMD64 Linux host")
class ToolchainProbeTest(unittest.TestCase):
    def setUp(self):
        self.tools = {role: shutil.which(name) for role, name in
                      (("cc", "gcc"), ("assembler", "as"), ("linker", "ld"))}
        if not all(self.tools.values()):
            self.skipTest("requires GCC and GNU binutils (as, ld)")
        directory = tempfile.TemporaryDirectory(prefix="on1x-toolchain-tests-")
        self.addCleanup(directory.cleanup)
        self.work = Path(directory.name)

    def wrapper(self, name, body):
        path = self.work / name
        path.write_text("#!/bin/sh\nset -eu\n" + body, encoding="ascii")
        path.chmod(0o700)
        return str(path)

    def probe(self, target=TARGET, env=None, **overrides):
        command = [sys.executable, "-B", SCRIPT, "--target", target]
        for role, path in (self.tools | overrides).items():
            command.extend(("--" + role, path))
        return subprocess.run(command, cwd=self.work, env=env, capture_output=True, timeout=40)

    def require_failure(self, result, text):
        self.assertEqual(result.returncode, 1, result)
        self.assertEqual(result.stdout, b"", result)
        self.assertIn(text, result.stderr)

    def require_manifest(self, result, cc=None):
        self.assertEqual(result.returncode, 0, result)
        self.assertEqual(result.stderr, b"", result)
        manifest = json.loads(result.stdout)
        self.assertEqual(manifest["schema"], "on1x.toolchain-probe/1")
        self.assertEqual(manifest["target"], TARGET)
        self.assertEqual(manifest["abi"], "SysVAMD64")
        self.assertEqual(manifest["object_format"], "ELF64")
        self.assertEqual(manifest["profile"], "linux-amd64-glibc-nonpie")
        sources = {"c_compiler": cc or self.tools["cc"],
                   "assembler": self.tools["assembler"], "linker": self.tools["linker"]}
        self.assertEqual(manifest["tools"].keys(), sources.keys())
        for role, source in sources.items():
            path = Path(source).resolve()
            self.assertEqual(manifest["tools"][role]["path"], str(path))
            self.assertEqual(manifest["tools"][role]["sha256"],
                             hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertTrue(manifest["tools"][role]["version"])

    def test_real_gcc_and_deterministic_output(self):
        first = self.probe()
        self.require_manifest(first)
        alias = self.work / "gcc-link"
        alias.symlink_to(self.tools["cc"])
        other_tmp = self.work / "another-tmp"
        other_tmp.mkdir()
        env = dict(os.environ, TMPDIR=str(other_tmp), PATH=str(self.work),
                   GCC_EXEC_PREFIX="/invalid/", COMPILER_PATH="/invalid")
        repeated = self.probe(env=env, cc=str(alias))
        self.require_manifest(repeated)
        self.assertEqual(first.stdout, repeated.stdout)

    def test_real_clang(self):
        clang = shutil.which("clang")
        if not clang:
            self.skipTest("Clang is not available")
        self.require_manifest(self.probe(cc=clang), cc=clang)

    def test_unsupported_target_does_not_run_tools(self):
        marker = self.work / "ran-tool"
        cc = self.wrapper("unexpected-execution", f'touch {shlex.quote(str(marker))}\n')
        self.require_failure(self.probe(target="aarch64-unknown-linux-gnu", cc=cc),
                             b"only native Linux")
        self.assertFalse(marker.exists())

    def test_relative_tool_has_no_path_fallback(self):
        self.require_failure(self.probe(assembler="as"), b"assembler: supply an absolute")

    def test_compiler_wrong_target(self):
        cc = self.wrapper("wrong-target", "printf 'i686-pc-linux-gnu\\n'\n")
        self.require_failure(self.probe(cc=cc), b"C compiler target mismatch")

    def test_compiler_target_banner_is_not_enough(self):
        gcc = shlex.quote(self.tools["cc"])
        cc = self.wrapper("wrong-char-abi", f'exec {gcc} -funsigned-char "$@"\n')
        self.require_failure(self.probe(cc=cc), b"C11 ABI compile: failed")

    def test_assembler_success_with_wrong_elf_class(self):
        assembler = shlex.quote(self.tools["assembler"])
        wrong = self.wrapper("wrong-assembler",
                             f'if [ "$1" = --version ]; then exec {assembler} "$@"; fi\n'
                             f"printf '.text\\nnop\\n' | {assembler} --32 -o sysv.o -\n")
        self.require_failure(self.probe(assembler=wrong), b"expected an AMD64 ELF64 relocatable")

    def test_linker_success_without_artifact(self):
        linker = shlex.quote(self.tools["linker"])
        wrong = self.wrapper("no-linker-output",
                             f'if [ "$1" = --version ]; then exec {linker} "$@"; fi\n'
                             "exit 0\n")
        self.require_failure(self.probe(linker=wrong),
                             b"relocatable link: expected an AMD64 ELF64 relocatable")

    def test_hosted_link_uses_selected_linker(self):
        linker = shlex.quote(self.tools["linker"])
        selected = self.wrapper("no-hosted-link",
                                f'case " $* " in\n'
                                f'  *" --version "*|*" -r "*) exec {linker} "$@" ;;\n'
                                'esac\nprintf "selected linker rejected hosted link\\n" >&2\nexit 9\n')
        for cc in filter(None, (self.tools["cc"], shutil.which("clang"))):
            with self.subTest(cc=cc):
                result = self.probe(cc=cc, linker=selected)
                self.require_failure(result, b"hosted link: failed")
                self.assertIn(b"selected linker rejected hosted link", result.stderr)

    def test_execute_detects_wrong_calling_convention(self):
        gcc = shlex.quote(self.tools["cc"])
        cc = self.wrapper("wrong-call-abi",
                          f'case " $* " in *" -c "*) exec {gcc} -mabi=ms "$@" ;; esac\n'
                          f'exec {gcc} "$@"\n')
        self.require_failure(self.probe(cc=cc), b"execute ABI fixture: failed")

    def test_changed_tool_does_not_publish_evidence(self):
        gcc = shlex.quote(self.tools["cc"])
        cc = self.wrapper("changed-cc", f'printf "# changed\\n" >> "$0"\nexec {gcc} "$@"\n')
        self.require_failure(self.probe(cc=cc), b"c_compiler: executable changed during probe")


if __name__ == "__main__":
    unittest.main()
