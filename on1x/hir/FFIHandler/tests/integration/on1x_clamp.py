#!/usr/bin/env python3
"""Black-box On1x clamp runtime, admission and no-publication controls."""

import argparse
import copy
from hashlib import sha256
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory


DSO = "libon1x_kernels.so"
MIN, MAX = -(2**63), 2**63 - 1


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("work", type=Path, help="new work/log directory")
    cli.add_argument("--driver", required=True, type=Path)
    cli.add_argument("--on1xc", required=True, type=Path)
    cli.add_argument("--cc", required=True)
    cli.add_argument("--mulib-root", type=Path,
                     help="omit to test the installed tool's native input discovery")
    cli.add_argument("--legacy-exotract", type=Path,
                     help="use the legacy compatibility CLI for comparison only")
    cli.add_argument("--expect-safe-dso-unsupported", action="store_true",
                     help="require explicit rejection; does not qualify safe DSO")
    a = cli.parse_args()
    work = a.work.resolve()
    work.mkdir(parents=True)
    tmp = work / "tmp"
    tmp.mkdir()
    empty_path = work / "runtime-no-tools"
    empty_path.mkdir()
    env = {key: val for key, val in os.environ.items()
           if not key.startswith(("LD_", "ON1X_"))}
    env.update(TMPDIR=str(tmp), LC_ALL="C")
    legacy = a.legacy_exotract is not None
    prefix = "kernel" if legacy else "ffi"
    base = [sys.executable, "-B", a.driver.resolve(strict=True),
            "--on1xc", a.on1xc.resolve(strict=True), "--cc", a.cc, "--no-prelude"]
    if a.mulib_root:
        base += ["--mulib-root", a.mulib_root.resolve(strict=True)]

    def run(tag, argv, *, status=0, diagnostic=None, runtime=False):
        command = list(map(str, argv))
        explicit_env = [f"PATH={empty_path}"] if runtime else []
        with (work / "commands.log").open("a") as log:
            print(f"{tag}: {shlex.join([*explicit_env, *command])}", file=log)
        p = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           timeout=150, cwd=work, env=({**env, "PATH": str(empty_path)} if runtime else env))
        (work / f"{tag}.stdout").write_bytes(p.stdout)
        (work / f"{tag}.stderr").write_bytes(p.stderr)
        if (not (p.returncode > 0 if status is None else p.returncode == status)
                or diagnostic is not None and diagnostic not in p.stderr):
            raise RuntimeError(f"{tag} exited {p.returncode}; see {work / (tag + '.stderr')}")
        return p.stdout

    if legacy:
        if a.expect_safe_dso_unsupported:
            cli.error("the qualified legacy profile expects safe DSO to work")
        resources = run("clang-resources", ["clang", "-print-resource-dir"]).decode().strip()
        base.extend(("--exotract", a.legacy_exotract.resolve(strict=True), "--resource-dir", resources))
    else:
        base += ["--ffi-provider", "host"]

    def source(tag, body, *, safe=False, dynamic=False):
        path = work / (tag + ".on1x")
        keyword = "import" if safe else "unsafe import"
        suffix = "[dynlib]" if dynamic else ""
        path.write_text(f"{keyword} kernel{suffix} mulib.clamp as C;\n"
                        "fn tap(x:i64)->i64 { println(x); return x; }\n"
                        f"fn main() {{ {body} }}\n")
        return path

    def build(tag, src, *, status=0, diagnostic=None, candidate=False, dynamic=False,
              extra=()):
        path = work / ("out-" + tag)
        mode = ([f"--{prefix}-review-candidate"] if candidate else
                ["--kernel-bind"] if legacy else [])
        dso_options = ["--kernel-dynlib-compiler=host"] if dynamic and legacy else []
        run(tag, [*base, *mode, *dso_options, *extra, src, "-o", path],
            status=status, diagnostic=diagnostic)
        if (status == 0) != path.exists():
            raise RuntimeError(f"{tag}: wrong output publication state")
        return path

    # Full-range cases and deterministic source order, independent of the native
    # driver's own qualification corpus and not a comparison against itself.
    def saturate(x):
        return min(MAX, max(MIN, x))

    calls = ["println(C.i64(tap(10), tap(20), tap(30)));"]
    values = [10, 20, 30, 20]
    for x, y in ((MAX, 1), (MIN, -1), (MAX, MIN), (MIN, MAX), (42, -15), (-12, 12)):
        calls.extend((f"println(C.add_i64({x}, {y}));", f"println(C.sub_i64({x}, {y}));"))
        values.extend((saturate(x + y), saturate(x - y)))
    calls.extend(("println(C.i64(-1, 0, 10));", "println(C.i64(20, -7, -7));"))
    values.extend((0, -7))
    body = " ".join(calls)
    expected = "".join(f"{value}\n" for value in values).encode()

    def compare(tag, program):
        if run(tag, [program], runtime=True) != expected:
            raise RuntimeError(f"{tag}: independent On1x oracle mismatch")

    static = source("unsafe", f"unsafe {{ {body} }}")
    if not legacy:
        inspection = build("inspection", static, extra=("--inspect-ffi",))
        report = json.loads(inspection.read_bytes())
        if (report.get("schema") != "on1x.ffi-plan/v1" or report.get("safe_export") is not False
                or report.get("admission") != "not_qualified"):
            raise RuntimeError("inspection is not the expected non-authoritative plan")
        build("unknown-provider", static, status=None, diagnostic=b"invalid choice",
              extra=("--ffi-provider", "qualification-unknown-provider"))
    compare("run-unsafe", build("unsafe", static))
    bad = source("bad-guard", "unsafe { println(C.i64(tap(0), tap(2), tap(1))); }")
    trapped = build("bad-guard", bad)
    if run("run-bad-guard", [trapped], status=70, runtime=True) != b"0\n2\n1\n":
        raise RuntimeError("reversed guard changed left-to-right once-only evaluation")
    unscoped = source("outside-unsafe", "println(C.add_i64(1, 2));")
    build("outside-unsafe", unscoped, status=None, diagnostic=b"CONTEXT")
    dyn = source("unsafe-dso", f"unsafe {{ {body} }}", dynamic=True)
    dyn_bundle = build("unsafe-dso", dyn, dynamic=True)
    if f"[{DSO}]".encode() not in run("dso-dynamic-tags", ["readelf", "-d", dyn_bundle / "program"]):
        raise RuntimeError("On1x executable does not dynamically depend on the expected DSO")
    moved = dyn_bundle.rename(work / "relocated-unsafe-dso")
    compare("run-unsafe-dso", moved / "program")
    shared = moved / DSO
    shared.rename(work / "withheld-unsafe-dso.so")
    run("missing-unsafe-dso", [moved / "program"], status=None, diagnostic=DSO.encode(),
        runtime=True)

    safe = source("safe", body, safe=True)
    build("unapproved", safe, status=None, diagnostic=b"NOT-ADMITTED")
    candidate = json.loads(build("candidate", safe, candidate=True).read_bytes())
    if candidate["schema"] != f"on1x.{prefix}-review-subject/v1":
        raise RuntimeError("unexpected review subject schema")

    # Keep private keys out of retained evidence (paths may appear in commands).
    with TemporaryDirectory(dir=tmp) as secret:
        secret = Path(secret)
        review, sig = secret / "review.json", secret / "review.sig"
        key, other_key = secret / "selected.der", secret / "wrong.der"
        private = secret / "selected.pem"
        for tag, pem, der in (("selected", private, key), ("wrong", secret / "wrong.pem", other_key)):
            run(f"key-{tag}", ["openssl", "genpkey", "-algorithm", "ED25519", "-out", pem])
            run(f"public-{tag}", ["openssl", "pkey", "-in", pem, "-pubout", "-outform", "DER",
                                   "-out", der])

        def sign(tag, subject):
            review.write_bytes(json.dumps(dict(schema=f"on1x.{prefix}-review/v1",
                                               reviewer="W11 ephemeral test authority",
                                               review_reference="TEST ONLY, no production approval",
                                               subject=subject)).encode())
            run(f"sign-{tag}", ["openssl", "pkeyutl", "-sign", "-rawin", "-inkey", private,
                                 "-in", review, "-out", sig])

        def params(public=key):
            return [f"--{prefix}-review", review, f"--{prefix}-review-key", public,
                    f"--{prefix}-review-signature", sig]

        def check_reviewed(tag, bundle):
            compare(tag, bundle / "program")
            admission = json.loads((bundle / "admission.json").read_bytes())
            for name in ("program", DSO) if (bundle / DSO).exists() else ("program",):
                if sha256((bundle / name).read_bytes()).hexdigest() != admission["artifacts"][name]:
                    raise RuntimeError(f"accepted artifact {name} does not match report")

        sign("candidate", candidate)
        check_reviewed("run-reviewed", build("reviewed", safe, extra=params()))
        build("wrong-authority", safe, extra=params(other_key), status=None,
              diagnostic=b"signature verification failed")
        build("compiler-flags-drift", safe, extra=(*params(), "--cflag=-O1"), status=None,
              diagnostic=b"does not match")
        # Valid JSON with a just one-byte change to the signed message.
        review.write_bytes(review.read_bytes() + b" ")
        build("mutated-review-bytes", safe, extra=params(), status=None,
              diagnostic=b"signature verification failed")
        changed = copy.deepcopy(candidate)
        changed["target"] = "aarch64-linux-gnu"
        sign("target-drift", changed)
        build("signed-stale-target", safe, extra=params(), status=None, diagnostic=b"does not match")
        safe_dyn = source("safe-dso", body, safe=True, dynamic=True)
        if a.expect_safe_dso_unsupported:
            build("candidate-safe-dso", safe_dyn, candidate=True, dynamic=True, status=None,
                  diagnostic=b"safe DSO load policy is not qualified")
        else:
            dynamic = json.loads(build("candidate-safe-dso", safe_dyn, candidate=True,
                                       dynamic=True).read_bytes())
            stale = copy.deepcopy(dynamic)
            if legacy:
                stale["dynlib"]["sha256"] = "0" * 64
            else:
                assert DSO in stale["artifacts"], stale["artifacts"]
                stale["artifacts"][DSO] = "0" * 64
            sign("artifact-drift", stale)
            build("signed-stale-dso", safe_dyn, extra=params(), dynamic=True, status=None,
                  diagnostic=b"does not match")
            sign("dynamic", dynamic)
            check_reviewed("run-reviewed-dso", build("reviewed-dso", safe_dyn,
                                                     extra=params(), dynamic=True))

    # Retain successful outputs including public reviews, never private key bytes.
    print("PASS unsafe/guard/scope, real relocated DSO, reviewed static and trust mutations")
    print("UNSUPPORTED safe DSO" if a.expect_safe_dso_unsupported else "PASS reviewed DSO and stale artifact")


if __name__ == "__main__":
    main()
