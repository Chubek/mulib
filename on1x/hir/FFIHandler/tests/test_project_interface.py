"""Provider transport and provenance tests; schema tests belong to W01."""

from dataclasses import replace
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from project_interface import CephyrProvider, Limits, ProjectionError


TARGET = "x86_64-linux-gnu"
REVISION = "e10ac69eed7baf91b06d15c4e0d58360097cb5c4"
WIRE = b'(interface 1\n  (target "x86_64-linux-gnu")\n  (data-model LP64)\n  (abi "SysVAMD64"))\n'
FAKE = r"""
import mmap
import os
from pathlib import Path
import sys
import time

args = sys.argv[1:]
if args == ["--version"]:
    print("Cephyr 0.1.0")
    sys.exit(0)
if args == ["--help"]:
    print("older compiler" if MODE == "old" else "--emit-interface=sexp")
    sys.exit(0)
assert args[-4:] == ["--emit-interface=sexp", "-x", "c", "-"]
assert args.count("--emit-interface=sexp") == 1
assert args[args.index("--config") + 1] == "Cephyr.toml"
assert Path("Cephyr.toml").read_text() == "# On1x projection: no ambient settings or extensions\n"
assert not {"CEPHYR_CONFIG", "CPATH", "PYTHONPATH", "XDG_CACHE_HOME"} & os.environ.keys()
assert "--stdlib=none" in args and "-nostdinc" in args
data = sys.stdin.buffer.read()
if MODE == "slow":
    if os.fork() == 0:
        sys.stderr.write("child waiting\n")
        sys.stderr.flush()
        time.sleep(30)
    sys.exit(0)
if MODE == "stdout":
    sys.stdout.buffer.write(b"x" * 4096)
elif MODE == "stderr":
    sys.stderr.buffer.write(b"x" * 4096)
elif MODE == "invalid":
    sys.stdout.buffer.write(b"this is not a projection\n")
elif MODE == "truncated":
    sys.stdout.buffer.write(WIRE[:-2])
elif MODE == "utf8":
    sys.stdout.buffer.write(WIRE[:-2] + b"\xff)\n")
elif MODE == "wrong-target":
    sys.stdout.buffer.write(WIRE.replace(b"x86_64-linux-gnu", b"aarch64-linux-gnu"))
elif MODE == "nonzero":
    sys.stdout.buffer.write(WIRE)
    sys.stderr.write("parser error\n")
    sys.exit(1)
else:
    if MODE == "memory":
        try:
            mmap.mmap(-1, 100 * 1024 * 1024)
        except OSError:
            sys.stderr.write("address-space cap enforced\n")
        else:
            sys.exit(1)
    if MODE == "header":
        assert data == b'#include "public.h"\n'
    sys.stderr.write("test diagnostic\n")
    sys.stdout.buffer.write(WIRE)
"""


class ProviderTests(unittest.TestCase):
    def setUp(self):
        work = tempfile.TemporaryDirectory()
        self.addCleanup(work.cleanup)
        self.local = Path(work.name).resolve()
        self.headers = self.local / "headers"
        self.headers.mkdir()
        (self.headers / "public.h").write_text('#include "dep.h"\n')
        (self.headers / "dep.h").write_text("int value(void);\n")

    def tool(self, mode):
        path = self.local / f"cephyr-{mode}"
        if not path.exists():
            path.write_text(f"#!{sys.executable}\nMODE = {mode!r}\nWIRE = {WIRE!r}\n" + FAKE)
            path.chmod(0o700)
        return path

    def provider(self, mode="ok", **kwargs):
        return CephyrProvider(self.tool(mode), TARGET, libstone_revision=REVISION,
                              scratch_root=self.local, include_dirs=(self.headers,), **kwargs)

    def reject(self, code, call):
        with self.assertRaises(ProjectionError) as failure:
            call()
        self.assertEqual(failure.exception.code, code)
        return failure.exception

    def test_explicit_argv_environment_and_header(self):
        (self.local / "bad.toml").write_text("not valid configuration")
        with patch.dict(os.environ, {"CEPHYR_CONFIG": str(self.local / "bad.toml"),
                                     "CPATH": "/invalid", "PYTHONPATH": "/invalid",
                                     "XDG_CACHE_HOME": "/invalid"}):
            provider = self.provider("header", defines=("FEATURE=2",), undefines=("STALE",))
            result = provider.project_header("public.h")
        self.assertEqual(result.output, WIRE)
        self.assertEqual(result.diagnostics, b"test diagnostic\n")
        self.assertEqual(result.provenance["identity"]["origin"], "unavailable")
        self.assertEqual(result.provenance["identity"]["header"], "public.h")
        self.assertEqual(result.provenance["identity"]["preprocessor"]["defines"], ["FEATURE=2"])
        argv = result.provenance["invocation"]["argv"]
        self.assertIn(f"--target={TARGET}", argv)
        self.assertIn("STALE", argv)
        self.assertNotIn(str(self.local / "bad.toml"), argv)
        self.assertEqual(result.provenance["invocation"]["exit_status"], 0)
        provider.verify_inputs()
        self.assertFalse(list(self.local.glob("on1x-cephyr-*")))

    def test_missing_and_old_tools_do_not_fall_back(self):
        self.reject("missing_input", lambda: CephyrProvider(
            self.local / "missing", TARGET, libstone_revision=REVISION, scratch_root=self.local))
        self.reject("unsupported_cephyr", lambda: self.provider("old"))

    def test_bad_input_and_profile_limits(self):
        provider = self.provider(limits=replace(Limits(), input_bytes=8))
        self.reject("input_limit", lambda: provider.project(b"x" * 9))
        self.reject("invalid_input", lambda: provider.project(b"x\0"))
        self.reject("invalid_input", lambda: provider.project(b".text", language="assembler"))
        self.reject("invalid_header", lambda: provider.project_header("../outside.h"))
        self.reject("invalid_header", lambda: provider.project_header('evil.h"\n#error injected'))
        self.reject("missing_header", lambda: provider.project_header("not-there.h"))
        self.reject("invalid_profile", lambda: self.provider(defines=("--config=elsewhere",)))
        self.reject("invalid_profile", lambda: self.provider(undefines=("X=3",)))
        self.reject("invalid_profile", lambda: self.provider(defines=("X=1" for _ in range(129))))
        self.reject("inventory_limit", lambda: self.provider(limits=replace(Limits(), inventory_entries=4)))

    def test_transport_failure_does_not_publish_output(self):
        for mode in ("invalid", "truncated", "utf8", "wrong-target"):
            with self.subTest(mode=mode):
                err = self.reject("invalid_output", lambda: self.provider(mode).project(b""))
                self.assertNotIn("projection_sha256", err.provenance["identity"])
        err = self.reject("cephyr_failed", lambda: self.provider("nonzero").project(b""))
        self.assertEqual(err.diagnostics, b"parser error\n")
        self.assertNotIn("projection_sha256", err.provenance["identity"])

    def test_enforces_output_diagnostic_and_timeout_limits(self):
        limits = replace(Limits(), output_bytes=200, diagnostic_bytes=32)
        self.reject("output_limit", lambda: self.provider("stdout", limits=limits).project(b""))
        err = self.reject("diagnostic_limit", lambda: self.provider("stderr", limits=limits).project(b""))
        self.assertEqual(len(err.diagnostics), limits.diagnostic_bytes)
        slow = self.provider("slow", limits=replace(Limits(), timeout_seconds=0.75))
        err = self.reject("timeout", lambda: slow.project(b""))
        self.assertEqual(err.diagnostics, b"child waiting\n")
        self.assertFalse(list(self.local.glob("on1x-cephyr-*")))

    def test_address_space_cap_is_inherited(self):
        capped = self.provider("memory", limits=replace(Limits(), address_space_bytes=64 * 1024 * 1024))
        self.assertIn(b"address-space cap enforced", capped.project(b"").diagnostics)

    def test_input_inventory_detects_transitive_edits_and_additions(self):
        provider = self.provider()
        first = provider.project_header("public.h").provenance["identity"]["roots"]
        (self.headers / "dep.h").write_text("long value(void);\n")
        self.reject("input_changed", provider.verify_inputs)
        provider = self.provider()
        second = provider.project_header("public.h").provenance["identity"]["roots"]
        self.assertNotEqual(first, second)
        (self.headers / "new.h").touch()
        self.reject("input_changed", provider.verify_inputs)

    def test_explicit_sysroot_is_hashed_without_adding_a_search_root(self):
        sysroot = self.local / "sysroot"
        includes = sysroot / "usr/include"
        includes.mkdir(parents=True)
        header = includes / "sysroot-only.h"
        header.write_bytes(b"int hidden(void);\n")
        # Legacy callers may also supply the sysroot as a tool input.
        provider = self.provider(sysroot=sysroot, tool_inputs=(sysroot,))
        first = provider.project(b"").provenance
        self.assertIn(f"--sysroot={sysroot}", first["invocation"]["argv"])
        self.assertEqual(first["invocation"]["selected_input_paths"]["sysroot"], str(sysroot))
        self.assertTrue(first["identity"]["preprocessor"]["sysroot_selected"])
        self.assertNotIn(str(sysroot), json.dumps(first["identity"]))
        self.assertEqual([r["role"] for r in first["identity"]["roots"]],
                         ["cephyr", "prlimit", "include:0", "sysroot"])
        self.reject("missing_header", lambda: provider.project_header("sysroot-only.h"))
        provider.verify_inputs()
        header.write_bytes(b"int edited(void);\n")
        self.reject("input_changed", provider.verify_inputs)
        second = self.provider(sysroot=sysroot).project(b"").provenance
        self.assertNotEqual(first["identity"]["roots"][-1]["sha256"],
                            second["identity"]["roots"][-1]["sha256"])
        self.reject("inventory_limit", lambda: self.provider(
            sysroot=sysroot, limits=replace(Limits(), inventory_entries=8)))

    def test_rejects_public_header_collision_between_distinct_selected_roots(self):
        alternate = self.local / "alternate"
        alternate.mkdir()
        primary = self.headers / "public.h"
        (alternate / primary.name).write_bytes(primary.read_bytes())
        provider = self.provider(system_include_dirs=(alternate,))
        self.reject("ambiguous_header", lambda: provider.check_public_header(primary.name))
        self.reject("ambiguous_header", lambda: provider.project_header(primary.name))
        # Repeating the same resolved root is not an independent competitor.
        provider = self.provider(system_include_dirs=(self.headers,))
        self.assertEqual(provider.check_public_header(primary.name), primary)

    def test_relocatable_identity_and_change_isolation(self):
        first = self.provider().project_header("public.h")
        alternate = self.local / "alternate"
        alternate.mkdir()
        for path in self.headers.iterdir():
            (alternate / path.name).write_bytes(path.read_bytes())
        provider = CephyrProvider(self.tool("ok"), TARGET, libstone_revision=REVISION,
                                  scratch_root=self.local, include_dirs=(alternate,))
        second = provider.project_header("public.h")
        self.assertEqual(first.provenance["identity"], second.provenance["identity"])
        self.assertNotEqual(first.provenance["invocation"], second.provenance["invocation"])
        self.assertNotIn(str(self.local), json.dumps(second.provenance["identity"]))
        second.provenance["identity"]["roots"].clear()
        provider.verify_inputs()

    def test_rejects_symlink_in_inventory_and_detects_tool_edits(self):
        provider = self.provider()
        with self.tool("ok").open("a") as stream:
            stream.write("# changed\n")
        self.reject("input_changed", lambda: provider.project(b""))
        (self.headers / "link.h").symlink_to(self.headers / "dep.h")
        self.reject("invalid_profile", self.provider)


if __name__ == "__main__":
    unittest.main()
