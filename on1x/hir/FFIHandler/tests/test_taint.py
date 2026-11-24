"""Focused Cephyr/Wetstone evidence and On1x diagnostic policy checks."""

import argparse
import os
from pathlib import Path
from subprocess import PIPE, run
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from cephyr_interface import decode_interface, InterfaceError
from ffgen import generate_module
from ffi_taint import callback_sites, wetstone_evidence
from project_interface import CephyrProvider
from test_ffgen import entry, TARGET

PIN = "e10ac69eed7baf91b06d15c4e0d58360097cb5c4"
SOURCE = b"""
typedef int (*mulib_x_cb)(int);
extern int mulib_x_object;
int outside_header_selection(int n);
int mulib_x_first(mulib_x_cb fn, int n) { return fn(n); }
int mulib_x_indirect(mulib_x_cb fn, int n) { return (*fn)(n); }
int mulib_x_other(mulib_x_cb fn, int n) { return n; }
int mulib_x_branch(mulib_x_cb fn, int n) { if (n) return fn(n); return n; }
int mulib_x_named_variadic(int n) { return n; }
int mulib_x_variadic(int n, ...);
int mulib_x_old();
_Noreturn void mulib_x_stop(void);
"""


def qualify_analysis(args, scratch):
    provider = CephyrProvider(args.cephyr, TARGET[0], libstone_revision=PIN, scratch_root=scratch,
                              tool_inputs=(*args.tool_input, args.wetstone))
    interface = decode_interface(provider.project(SOURCE).output, expected_target=TARGET[0])
    syntax = provider.project_syntax(SOURCE)
    replay = provider.project(syntax.output, language="c11-ast-sexp")
    assert decode_interface(replay.output, expected_target=TARGET[0]) == interface
    functions = {fn.name: fn for fn in interface.functions}
    sites = callback_sites(syntax.output, functions)
    assert sites == {"mulib_x_first": {"parameter": 0, "preprocessed_line": 5},
                     "mulib_x_indirect": {"parameter": 0, "preprocessed_line": 6}}, sites
    evidence = wetstone_evidence(provider, args.wetstone, functions, sites)
    assert evidence == sites
    provider.verify_inputs()
    generated = generate_module(entry(), interface, TARGET, callback_taint=evidence,
                                body_taint={"status": "partial"})
    records = {row["symbol"]: row for row in generated.metadata["symbols"]}

    def facts(name):
        return {e["code"] for e in records["mulib_x_" + name]["taint"]}

    assert facts("first") == facts("indirect") == {"taint_function_pointer", "taint_caller_callback"}
    assert facts("branch") == facts("other") == {"taint_function_pointer"}
    assert facts("named_variadic") == set()
    assert facts("variadic") == {"variadic"}
    assert facts("old") == {"no_prototype"}
    assert facts("stop") == {"noreturn"}
    assert b"profile scalar" not in generated.document
    assert b"reason taint_caller_callback" in generated.document
    projection = {row["symbol"]: row for row in generated.metadata["projection_only"]}
    assert projection["mulib_x_cb"]["kind"] == "typedef"
    assert projection["mulib_x_object"]["kind"] == "object"
    assert projection["outside_header_selection"]["reason"] == "outside_module_selection"
    for malformed in (b'(2 (object))', b'(1 (object ("ast" null) ("ast" null) ("source" null)))'):
        try:
            callback_sites(malformed, functions)
        except InterfaceError:
            pass
        else:
            raise AssertionError("invalid C11Syntax projection accepted")
    print("PASS real Cephyr AST/interface and Wetstone lower bound; inventory/non-authority negatives")


def qualify_policy(args, scratch):
    source = scratch / "main.on1x"
    source.write_text("unsafe import kernel mulib.avl as Avl;\n"
                      "fn main() -> i64 { return unsafe { Avl.foreach(1) }; }\n")
    output = scratch / "main.c"

    def reject(command, *, taint):
        result = run(command, cwd=scratch, stdout=PIPE, stderr=PIPE, timeout=30, check=False)
        assert result.returncode and not output.exists(), result
        assert (b"ON1X-FFI-TAINT" in result.stderr) == taint, result.stderr.decode(errors="replace")
        return result.stderr

    command = [args.on1xc, "--emit-c", "--no-prelude", "--kernel-c11", "--ffi-stage",
               source, "-o", output]
    assert b"taint_function_pointer" in reject(command, taint=True)
    reject([*command, "--ffi-taint", "allow"], taint=False)
    source.write_text("unsafe import kernel mulib.clamp as Clamp;\n"
                      "fn main() -> i64 { return unsafe { Clamp.unknown(1) }; }\n")
    assert b"ON1X-KERNEL-MEMBER" in reject([*command, "--ffi-taint", "allow"], taint=False)
    source.write_text("import kernel mulib.clamp as Clamp;\n"
                      "fn main() -> i64 { return Clamp.add_i64(1, 2); }\n")
    build = Path(__file__).resolve().parents[3] / "tools/on1x-build.py"
    assert b"ON1X-FFI-NOT-ADMITTED" in reject(
        [sys.executable, build, source, "-o", output, "--on1xc", args.on1xc,
         "--mulib-root", args.mulib_root, "--ffi-provider", "host", "--ffi-taint", "allow"],
        taint=False)

    source.write_text("unsafe import kernel mulib.avl as Avl;\n"
                      "fn main() -> i64 { return unsafe { Avl.foreach(1) }; }\n")
    manifest = scratch / "On1xfile"
    root = 'on1xfile 1\nkind executable\nname "policy"\ndirectory "."\ntarget "policy" "main.on1x"\n'
    manifest.write_text(root)
    (scratch / "On1xfile.m").write_text('on1xfile 1\nsource "main.on1x"\n')
    project = [args.on1xc, "aux", "plan", "--on1xc", args.on1xc, "--ffi-provider", "host",
               "--mulib-root", args.mulib_root]
    reject(project, taint=True)
    reject([*project, "--ffi-taint", "allow"], taint=False)
    manifest.write_text(root + "ffi-taint allow\n")
    reject(project, taint=False)
    reject([*project, "--ffi-taint", "reject"], taint=True)
    manifest.write_text(root)
    (scratch / "On1xfile.m").write_text('on1xfile 1\nsource "main.on1x"\nffi-taint allow\n')
    assert b"ffi-taint" in reject(project, taint=False)
    print("PASS fail-closed direct/project policy, both precedence directions, no output or unknown admission")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cephyr", type=Path)
    p.add_argument("--wetstone", type=Path)
    p.add_argument("--tool-input", type=Path, action="append", default=[])
    p.add_argument("--on1xc", type=Path)
    p.add_argument("--mulib-root", type=Path)
    p.add_argument("--scratch", type=Path, default=Path(os.path.abspath(os.curdir)))
    args = p.parse_args()
    if (bool(args.cephyr) != bool(args.wetstone) or bool(args.on1xc) != bool(args.mulib_root)
            or not (args.cephyr or args.on1xc)):
        p.error("select Cephyr + Wetstone and/or on1xc + MuLIB root")
    with TemporaryDirectory(prefix="on1x-ffi-taint-", dir=args.scratch) as name:
        if args.cephyr:
            qualify_analysis(args, Path(name))
        if args.on1xc:
            qualify_policy(args, Path(name))


if __name__ == "__main__":
    main()
