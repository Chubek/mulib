#!/usr/bin/env python3
"""Exercise status generation, rejection, and a real Cephyr embedding call."""

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ON1X = HERE.parents[2]
TOOLS = HERE.parent / "tools"
FIXTURE = HERE / "foreign_conditions"
sys.path.insert(0, str(TOOLS))
from ffi_conditions import MAX_CONTRACT_BYTES, generate


def wire(contract, projection=None):
    if projection is not None:
        contract = {**contract, "interface_sha256": sha256(projection).hexdigest()}
    return json.dumps(contract).encode("ascii")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--cephyr", required=True, type=Path)
    cli.add_argument("--libcephyr", required=True, type=Path)
    cli.add_argument("--libstone-root", required=True, type=Path)
    cli.add_argument("--on1xc", required=True, type=Path)
    cli.add_argument("--work", required=True, type=Path, help="new output directory")
    cli.add_argument("--cc", default="cc")
    cli.add_argument("--cflag", action="append", default=[])
    args = cli.parse_args()
    if not __debug__:
        cli.error("assertions must be enabled")
    work = args.work.resolve()
    work.mkdir(parents=True)
    config = work / "Cephyr.toml"
    config.write_text("# No ambient Cephyr config or extensions.\n", encoding="ascii")
    cephyr = args.cephyr.resolve(strict=True)
    libcephyr = args.libcephyr.resolve(strict=True)
    cephyr_include = args.libstone_root.resolve(strict=True) / "cornerstone/Cephyr/include"
    on1xc = args.on1xc.resolve(strict=True)
    env = {key: val for key, val in os.environ.items() if not key.startswith("CEPHYR_")}
    env.update(CEPHYR_CONFIG=str(config), TMPDIR=str(work), LC_ALL="C")

    def run(label, command, data=None, expect=0):
        with (work / "commands.log").open("a", encoding="utf-8") as log:
            print(label + ": " + shlex.join(map(str, command)), file=log)
        proc = subprocess.run(command, input=data, cwd=work, env=env,
                              capture_output=True, timeout=90)
        (work / (label + ".stdout")).write_bytes(proc.stdout)
        (work / (label + ".stderr")).write_bytes(proc.stderr)
        assert proc.returncode == expect, (label, proc.returncode, proc.stderr.decode(errors="replace"))
        return proc

    def project(label, source, target="x86_64-linux-gnu"):
        return run(label, [cephyr, "--config", config, f"--target={target}", "--stdlib=cephyr",
                           "--emit-interface=sexp", "-I", cephyr_include, "-x", "c", "-"],
                   source).stdout

    projection = project("cephyr-header", b"#include <cephyr/Cephyr.h>\n")
    source_path = work / "cephyr.interface"
    source_path.write_bytes(projection)
    contract_path = FIXTURE / "cephyr_status.json"
    generator = [sys.executable, "-B", TOOLS / "ffi_conditions.py", source_path]
    generated = run("generate", [*generator, contract_path]).stdout
    (work / "CephyrCondition.on1x").write_bytes(generated)
    contract = json.loads(contract_path.read_bytes())
    assert generated == generate(projection, wire(contract)).encode("ascii")
    assert generated == generate(projection, wire(dict(reversed(list(contract.items()))))).encode("ascii")

    rejected = 0

    def reject(doc, raw, message):
        nonlocal rejected
        try:
            generate(doc, raw)
        except ValueError as exc:
            assert message in str(exc), (message, str(exc))
            rejected += 1
        else:
            raise AssertionError(f"accepted {message} control")

    for changed, message in (
        ({"interface_sha256": "0" * 64}, "SHA-256"),
        ({"symbol": "cephyr_result_output"}, "C int function"),  # no NULL inference
        ({"symbol": "cephyr_abi_version"}, "C int function"),   # not same-width unsigned
        ({"enum": "not_a_tag"}, "complete named status enum"),
        ({"success": "CEPHYR_NO_SUCH_STATUS"}, "enumerator missing"),
        ({"failures": {}}, "failure mappings"),
        ({"failures": {"CEPHYR_OK": "AlsoOk"}}, "overlapping status"),
        ({"failures": {"CEPHYR_LOAD_ERROR": "UnrecognizedStatus"}}, "reserved condition"),
        ({"failures": {"CEPHYR_LOAD_ERROR": "Escape} # injected"}}, "invalid On1x condition"),
        ({"failures": {"CEPHYR_LOAD_ERROR": "Error", "CEPHYR_COMPILE_ERROR": "Error"}}, "duplicate"),
        ({"version": True}, "version"),
        ({"errno": "EIO"}, "exact status contract"),
        ({"null": "NullValue"}, "exact status contract"),
        ({"unwind": "caught"}, "exact status contract"),
    ):
        reject(projection, wire({**contract, **changed}), message)
    reject(projection, b'{"version":1,"version":1}', "duplicate member")
    reject(projection, b" " * (MAX_CONTRACT_BYTES + 1), "byte limit")
    reject(projection + projection, wire(contract, projection + projection), "trailing form")
    stale = work / "stale.json"
    stale.write_bytes(wire({**contract, "interface_sha256": "0" * 64}))
    denied = run("stale-cli", [*generator, stale], expect=1)
    assert not denied.stdout and b"SHA-256" in denied.stderr

    probe = b"""
        enum Status { Good = 0, Bad = -1, AlsoBad = -1 };
        int returned(void);
        int unprototyped();
        int variadic(int value, ...);
        _Noreturn int terminal(void);
        static int hidden(void) { return Bad; }
    """.strip() + b"\n"
    doc = project("projection-controls", probe)
    example = {"version": 1, "symbol": "returned", "enum": "Status", "success": "Good",
               "failures": {"Bad": "Failure"}}
    assert "status == -1" in generate(doc, wire(example, doc))
    for symbol in ("unprototyped", "variadic", "terminal", "hidden"):
        reject(doc, wire({**example, "symbol": symbol}, doc), "C int function")
    reject(doc, wire({**example, "failures": {"Bad": "Failure", "AlsoBad": "Alias"}}, doc),
           "overlapping status")
    damaged = doc.replace(b'(enumerator "Bad" -1)', b'(enumerator "Bad" -2147483649)')
    assert damaged != doc
    reject(damaged, wire(example, damaged), "invalid numeric field")
    other = project("wrong-target", probe, "aarch64-linux-gnu")
    reject(other, wire(example, other), "target")

    # Identical generated enums from different modules cannot share an error edge.
    (work / "OtherCondition.on1x").write_bytes(generated)
    wrong = work / "wrong_condition.on1x"
    wrong.write_text("""from std.core.Pervasive import Result;
import CephyrCondition as A;
import OtherCondition as B;
fn mismatched() -> Result<i64, B.ForeignCondition> {
    let v = A.signal_status(1)?;
    return variant<Result<i64, B.ForeignCondition>>(Ok, v);
}
fn main() { let r = mismatched(); }
""", encoding="ascii")
    rejected_program = run("wrong-nominal", [on1xc, "--stdlib", ON1X / "stdlib", "--no-prelude",
                                            wrong, "-o", work / "wrong.c"], expect=1)
    assert b"incompatible nominal types" in rejected_program.stderr and not (work / "wrong.c").exists()

    binary = work / "client"
    run("build-client", [sys.executable, "-B", ON1X / "tools/on1x-build.py", FIXTURE / "client.on1x",
                         "--on1xc", on1xc, "--cc", args.cc, "--stdlib", ON1X / "stdlib",
                         "--no-prelude", "-I", work, "--extra-c", FIXTURE / "provider.c",
                         f"--cflag=-I{cephyr_include}", f"--cflag=-Wl,-rpath,{libcephyr.parent}",
                         f"--cflag={libcephyr}", *(f"--cflag={flag}" for flag in args.cflag),
                         "-o", binary])
    client = run("run-client", [binary])
    assert client.stdout == b"2\n1\n3\n" * 3 + b"42\n"
    assert not client.stderr, client.stderr
    print(f"PASS {rejected + 2} rejection controls, real Cephyr success/parse error/API misuse, "
          "three one-call paths with LIFO defer and exactly-once owner cleanup")


if __name__ == "__main__":
    main()
