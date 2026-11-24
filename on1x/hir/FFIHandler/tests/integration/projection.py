#!/usr/bin/env python3
"""Independently probe real Cephyr C11Interface; this does not execute target code."""

import argparse
from hashlib import sha256
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import sys


HERE = Path(__file__).resolve().parent
TARGETS = {"x86_64-linux-gnu": ("SysVAMD64", "sysv"),
           "aarch64-linux-gnu": ("AAPCS64", "aapcs64"),
           "riscv64-linux-gnu": ("LP64", "lp64")}
STAGE = "--emit-interface=sexp"


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("work", type=Path, help="new directory for artifacts/logs")
    cli.add_argument("--cephyr", type=Path, required=True)
    cli.add_argument("--libstone-root", type=Path, required=True)
    cli.add_argument("--include", type=Path, default=HERE.parents[4] / "include")
    cli.add_argument("--reader-tools", type=Path, help="also exercise W01 reader with actual outputs")
    cli.add_argument("--allow-media-missing-math", action="store_true",
                     help="accept only the known explicit missing math.h diagnostic")
    a = cli.parse_args()
    if not __debug__:
        cli.error("assertions must be enabled")
    cephyr = a.cephyr.resolve(strict=True)
    libstone = a.libstone_root.resolve(strict=True)
    include = a.include.resolve(strict=True)
    upstream = libstone / "cornerstone/Cephyr/tests/test_c11_interface.py"
    upstream_api = runpy.run_path(str(upstream))
    parse, value, named, one = (upstream_api[name] for name in ("parse", "value", "named", "one"))
    decode = None
    if a.reader_tools:
        sys.path.insert(0, str(a.reader_tools.resolve(strict=True)))
        from cephyr_interface import InterfaceError, decode_interface
        decode = decode_interface

    root = a.work.resolve()
    root.mkdir(parents=True)
    tmp = root / "tmp"
    tmp.mkdir()
    config = root / "Cephyr.toml"
    config.write_text("[blackstone]\nenabled = false\n")
    env = {key: val for key, val in os.environ.items() if not key.startswith("CEPHYR_")}
    env.update(TMPDIR=str(tmp), LC_ALL="C")
    for header in ("clamp", "media-timebase", "container-core"):
        if not (include / f"mulib-{header}.h").is_file():
            cli.error(f"real MuLIB header missing: mulib-{header}.h")

    def run(tag, args, *, data=None, allow_failure=False):
        with (root / "commands.log").open("a") as log:
            print(f"{tag}: {shlex.join(map(str, args))}", file=log)
        p = subprocess.run(args, input=data, capture_output=True, timeout=90, env=env, cwd=root)
        (root / f"{tag}.stdout").write_bytes(p.stdout)
        (root / f"{tag}.stderr").write_bytes(p.stderr)
        if p.returncode != 0 and not allow_failure:
            raise RuntimeError(f"{tag} exited {p.returncode}; see {root / (tag + '.stderr')}")
        return p

    def identity(target, payload):
        doc = parse(payload)
        assert doc[:2] == ["interface", 1]
        assert value(doc, "target") == target and value(doc, "abi") == TARGETS[target][0]
        assert value(doc, "data-model") == "LP64"
        return doc

    def signature(doc, target, name, linkage, definition, count, primitive, signed):
        fn = named(doc, "function", name)
        assert value(fn, "linkage") == linkage and value(fn, "definition") == definition, fn
        assert value(fn, "calling-convention") == TARGETS[target][1], fn
        if linkage == "internal":
            assert "inline" in one(fn, "specifiers")[1:], fn
        sig = value(fn, "type")
        assert value(sig, "prototype") == "true" and value(sig, "variadic") == "false", sig
        params = one(sig, "parameters")[1:]
        assert len(params) == count, sig
        for t in (value(sig, "result"), *params):
            assert t[:2] == ["integer", primitive], t
            assert (value(t, "rank"), value(t, "bits"), value(t, "size"), value(t, "align"),
                    value(t, "signed")) == (4, 64, 8, 8, signed), t

    run("version", [cephyr, "--version"])
    run("revision", ["git", "-C", libstone, "rev-parse", "HEAD"])
    run("checkout-status", ["git", "-C", libstone, "status", "--short"])
    (root / "input-identities.log").write_text("".join(
        f"{sha256(path.read_bytes()).hexdigest()}  {path}\n"
        for path in (cephyr, upstream, include / "mulib-clamp.h",
                     include / "mulib-container-core.h", include / "mulib-media-timebase.h")))
    run("upstream-interface", [sys.executable, "-B", upstream, cephyr])
    print("PASS upstream Cephyr C11Interface script (three targets; AMD64 input-route parity and negatives)",
          flush=True)

    for target in TARGETS:
        base = [cephyr, "--config", config, f"--target={target}", "--stdlib=cephyr", "-I", include]

        def compile(tag, *flags, data, allow_failure=False):
            return run(f"{target}-{tag}", [*base, *flags, "-"], data=data,
                       allow_failure=allow_failure)

        header = b"#include <mulib-clamp.h>\n"
        raw = compile("clamp", STAGE, "-x", "c", data=header).stdout
        preprocessed = compile("clamp-preprocess", "-E", "-x", "c", data=header).stdout
        pp = compile("clamp-from-cpp", STAGE, "-x", "cpp-output", data=preprocessed).stdout
        syntax = compile("clamp-syntax", "--emit-ast=json", "-x", "c", data=header).stdout
        ast = compile("clamp-from-syntax", STAGE, "-x", "c11-ast-json", data=syntax).stdout
        doc = identity(target, raw)
        assert doc == parse(pp) == parse(ast)
        for name, count in (("mulib_clamp_i64", 3), ("mulib_clamp_add_i64", 2),
                            ("mulib_clamp_sub_i64", 2)):
            signature(doc, target, name, "external", "declaration", count, "long", "true")
        print(f"PASS {target}: real clamp, exact native types, three identical parsed routes",
              flush=True)

        if decode:
            assert decode(raw, expected_target=target) == decode(pp, expected_target=target)
            assert decode(raw, expected_target=target) == decode(ast, expected_target=target)
            wrong = next(candidate for candidate in TARGETS if candidate != target)
            changed, replaced = raw.replace(b"(interface 1", b"(interface 2", 1), raw.rstrip()
            assert changed != raw and replaced.endswith(b")")
            for bad, expected, code in (
                (raw, wrong, "target"), (changed, target, "version"),
                (raw + raw, target, "syntax"), (replaced[:-1], target, "syntax"),
                (raw + b"\xff", target, "syntax"),
            ):
                try:
                    decode(bad, expected_target=expected)
                except InterfaceError as exc:
                    assert exc.code == code, exc
                else:
                    raise AssertionError(f"reader admitted invalid {code} input")
            print(f"PASS {target}: consumer route parity and 5 fail-closed wire controls", flush=True)

        raw = compile("container-core", STAGE, "-x", "c",
                      data=b"#include <mulib-container-core.h>\n").stdout
        signature(identity(target, raw), target, "mulib_ct_counter_add",
                  "internal", "defined", 2, "unsigned-long", "false")
        print(f"PASS {target}: real container-core unsigned static inline declaration", flush=True)
        media = compile("media-timebase", STAGE, "-x", "c",
                        data=b"#include <mulib-media-timebase.h>\n",
                        allow_failure=a.allow_media_missing_math)
        if media.returncode:
            assert media.returncode == 1 and not media.stdout and b"file 'math.h' not found" in media.stderr
            print(f"UNSUPPORTED {target}: media-timebase needs math.h in this Cephyr stdlib", flush=True)
        else:
            signature(identity(target, media.stdout), target, "mulib_media_timebase_gcd",
                      "internal", "defined", 2, "unsigned-long", "false")
            print(f"PASS {target}: real media-timebase unsigned static inline declaration", flush=True)


if __name__ == "__main__":
    main()
