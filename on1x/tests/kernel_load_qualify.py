#!/usr/bin/env python3
"""Qualify the load-time clamp product against an independent integer oracle.

For the relocated-install gate, supply a fully built CMake tree and two clamp
packages minted separately (one recipe-only, one native). Authorize only the
test native build with an explicit host policy outside the package roots:

    python3 kernel_load_qualify.py /fresh/work --install-build /cmake/tree \
        --recipe /portable/stdffi.clamp.on1xkp --native /native/stdffi.clamp.on1xkp \
        --policy /trusted/policy

Supply --baseline ON1XC to also run the build-pinned interpreter and VM.
Supply --cc clang and --cflag options for a separately instrumented C run.
"""

import argparse
from hashlib import sha256
import itertools
import os
from pathlib import Path
import random
import re
import shutil
import struct
import subprocess
import sys


I64_MIN = -(1 << 63)
I64_MAX = (1 << 63) - 1
POINTS = (I64_MIN, I64_MIN + 1, -2, -1, 0, 1, 2, I64_MAX - 1, I64_MAX)
IMPORT = "import foreign stdffi.clamp as K;\n"
GUARD = IMPORT + """\
fn mark(n: i64) -> i64 { println(n); return n; }
fn main() {
  println(K.i64(mark(1), mark(3), mark(2)));
  println(17001);
}
"""
# Checked clamp v1 expressions, with only parameter names replaced by the
# format's positional spelling. These are not used to compute test results.
BINDINGS = (
    ("add_i64", 2, "true",
     "(if (and (gt $1 0) (gt $0 (sub 9223372036854775807 $1))) 9223372036854775807 "
     "(if (and (lt $1 0) (lt $0 (sub -9223372036854775808 $1))) -9223372036854775808 (add $0 $1)))"),
    ("i64", 3, "(le $1 $2)", "(if (lt $0 $1) $1 (if (gt $0 $2) $2 $0))"),
    ("sub_i64", 2, "true",
     "(if (and (lt $1 0) (gt $0 (add 9223372036854775807 $1))) 9223372036854775807 "
     "(if (and (gt $1 0) (lt $0 (add -9223372036854775808 $1))) -9223372036854775808 (sub $0 $1)))"),
)


def cases():
    # Python integers have no overflow; do not duplicate the implementation's
    # overflow predicates or derive expected values from the checked recipe.
    for a, b in itertools.product(POINTS, repeat=2):
        yield "add_i64", (a, b), max(I64_MIN, min(I64_MAX, a + b))
        yield "sub_i64", (a, b), max(I64_MIN, min(I64_MAX, a - b))
    for lo, hi in itertools.combinations_with_replacement(POINTS, 2):
        for x in (I64_MIN, lo, (lo + hi) // 2, hi, I64_MAX):
            yield "i64", (x, lo, hi), min(hi, max(lo, x))

    rng = random.Random(0x10A1)
    for _ in range(64):
        a, b, c = (rng.randrange(I64_MIN, I64_MAX + 1) for _ in range(3))
        lo, hi = sorted((b, c))
        yield "add_i64", (a, b), max(I64_MIN, min(I64_MAX, a + b))
        yield "sub_i64", (a, b), max(I64_MIN, min(I64_MAX, a - b))
        yield "i64", (a, lo, hi), min(hi, max(lo, a))


def write_programs(work):
    vectors = list(cases())
    source = work / "differential.on1x"
    guard = work / "guard.on1x"
    # The marker distinguishes admission failures from errors after user code
    # begins. The two- and three-argument paths all use the same executable.
    lines = [IMPORT, "fn main() {", "  println(17000);"]
    for name, operands, _ in vectors:
        arguments = ", ".join(str(value) for value in operands)
        lines.append(f"  println(K.{name}({arguments}));")
    source.write_text("\n".join([*lines, "}", ""]), encoding="ascii")
    guard.write_text(GUARD, encoding="ascii")
    expected = "17000\n" + "".join(f"{value}\n" for _, _, value in vectors)
    (work / "differential.expected").write_text(expected, encoding="ascii")
    return source, guard, vectors, expected


def run(command, *, cwd, env):
    return subprocess.run(command, cwd=cwd, env=env, text=True,
                          capture_output=True, timeout=180)


def clean_env():
    # Relocated-tool runs must not accidentally use ambient checkout paths or
    # a previous activation's package roots and authorization policy.
    return {key: value for key, value in os.environ.items()
            if not key.startswith(("ON1X_", "MULIB_"))}


def require(predicate, label, result):
    if not predicate:
        raise RuntimeError(f"{label}: exit={result.returncode}\n"
                           f"stdout={result.stdout[:1000]!r}\nstderr={result.stderr!r}")


def check_values(label, result, vectors, expected):
    require(result.returncode == 0, label, result)
    if result.stdout == expected:
        return
    for i, (actual, wanted) in enumerate(itertools.zip_longest(
            result.stdout.splitlines(), expected.splitlines())):
        if actual != wanted:
            case = vectors[i - 1][:2] if 1 <= i <= len(vectors) else "trace"
            raise RuntimeError(f"{label}: line {i + 1}, case {case}: "
                               f"expected {wanted!r}, got {actual!r}")
    raise RuntimeError(f"{label}: output differed in line endings")


def checked(command, cwd, env):
    result = run(command, cwd=cwd, env=env)
    require(result.returncode == 0, f"command {command}", result)
    return result


def expected_bindings():
    for name, arity, guard, body in BINDINGS:
        signature = ",".join(["i64"] * arity)
        interface = (f"on1x.kernel.interface/v1\nmodule=stdffi.clamp\n"
                     f"export={name}\nsignature=({signature})->i64\n")
        behavior = f"(guard {guard})\n(body {body})\n"
        yield name, arity, sha256(interface.encode("ascii")).digest(), sha256(
            behavior.encode("ascii")).digest()


def check_package_identity(data, provider):
    # This checks exactly the fixture shape, not untrusted package admission.
    def wire_name(name):
        return bytes((len(name),)) + name.encode("ascii")

    kind = 1 if provider == "recipe" else 2
    header = struct.Struct(">8sBBBBBIII")
    if len(data) < header.size:
        raise RuntimeError(f"{provider} lacks a complete package header")
    actual = header.unpack_from(data)
    recipe_size, image_size = actual[-2:]
    if actual[:-2] != (b"ON1XKP\r\n", 1, kind, 3, 0, 0, 1):
        raise RuntimeError(f"{provider} package has unexpected clamp v1 header: {actual}")
    prefix = wire_name("stdffi.clamp")
    for name, arity, interface, behavior in expected_bindings():
        prefix += wire_name(name) + bytes((arity,)) + interface + behavior
    if data[header.size:header.size + len(prefix)] != prefix:
        raise RuntimeError(f"{provider} wire export identities differ from the independent baseline")
    payload_start = header.size + len(prefix)
    digest_size = 32 if kind == 2 else 0
    if not recipe_size or bool(image_size) != (kind == 2):
        raise RuntimeError(f"{provider} package has unexpected payload sizes")
    image_start = payload_start + digest_size + recipe_size
    if len(data) != image_start + image_size:
        raise RuntimeError(f"{provider} package lengths do not account for the entire fixture")
    if kind == 2 and sha256(data[image_start:]).digest() != data[payload_start:payload_start + 32]:
        raise RuntimeError("native image SHA-256 does not cover the exact payload bytes")


def check_emitted_identity(compiler, source, work, env):
    generated = work / "requirements.c"
    checked([compiler, "--emit-c", "--kernel-load-time", source, "-o", generated], work, env)
    text = generated.read_text(encoding="utf-8")
    records = re.findall(
        r'\{\s*\.module\s*=\s*"stdffi\.clamp"\s*,\s*\.export_name\s*=\s*"([a-z_0-9]+)"\s*,'
        r'\s*\.version\s*=\s*(\d+)[uU]?\s*,\s*\.arity\s*=\s*(\d+)[uU]?\s*,'
        r'\s*\.interface_sha256\s*=\s*"([0-9a-f]{64})"\s*,'
        r'\s*\.behavior_sha256\s*=\s*"([0-9a-f]{64})"\s*\}', text)
    expected = [(name, "1", str(arity), interface.hex(), behavior.hex())
                for name, arity, interface, behavior in expected_bindings()]
    if sorted(records) != expected:
        raise RuntimeError(f"generated C does not pin the expected clamp v1 bindings: {records}")
    dispatch = re.findall(r"\bon1x_kernel_call([23])\s*\(", text)
    if sorted(dispatch) != ["2", "2", "3"] or "on1x_kernel_rt_builtin_i64" in text:
        raise RuntimeError(f"generated C does not use the expected typed slots: {dispatch}")


def isolated_package(work, path, provider):
    root = work / f"{provider}-only"
    root.mkdir()
    package = root / "stdffi.clamp.on1xkp"
    shutil.copyfile(path.resolve(strict=True), package)
    data = package.read_bytes()
    check_package_identity(data, provider)
    return root, sha256(data).hexdigest()


def qualify_load_time(args, work, source, guard, vectors, expected):
    env = clean_env()
    if args.install_build:
        build = args.install_build.resolve(strict=True)
        staging = work / "install-staging"
        installed = work / "install-relocated"
        checked(["cmake", "--install", build, "--prefix", staging], work, env)
        staging.rename(installed)
        compiler = installed / "bin/on1xc"
        builder = installed / "share/on1x/tools/on1x-build.py"
    else:
        compiler = args.compiler.resolve(strict=True)
        builder = args.builder.resolve(strict=True)

    check_emitted_identity(compiler, source, work, env)
    executable = work / "differential"
    guard_executable = work / "guard"
    for program, output in ((source, executable), (guard, guard_executable)):
        command = [sys.executable, "-B", builder, program, "--on1xc", compiler,
                   "--cc", args.cc, "--kernel-load-time", "-o", output]
        command.extend(f"--cflag={flag}" for flag in args.cflag)
        checked(command, work, env)

    empty = work / "empty-root"
    empty.mkdir()
    cwd = work / "empty-cwd"
    cwd.mkdir()
    # Runtime execution does not need a checkout, PATH, or loader overrides.
    env = {key: env[key] for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS", "LSAN_OPTIONS")
           if key in env}
    for label, override in (("unset roots", {}), ("empty roots", {"ON1X_KERNEL_PACKAGE_ROOTS": ""}),
                            ("root without package", {"ON1X_KERNEL_PACKAGE_ROOTS": str(empty)})):
        bad = run([executable], cwd=cwd, env={**env, **override, "ON1X_KERNEL_TRACE": "1"})
        require(bad.returncode > 0 and not bad.stdout and "stdffi.clamp" in bad.stderr
                and "kernel provider" not in bad.stderr, label, bad)
    print("PASS load-time admission: absent package fails before entering user code")

    binary_digest = sha256(executable.read_bytes()).hexdigest()
    for provider, path in (("recipe", args.recipe), ("native", args.native)):
        if path is None:
            continue
        root, digest = isolated_package(work, path, provider)
        activation = {**env, "ON1X_KERNEL_PACKAGE_ROOTS": str(root), "ON1X_KERNEL_TRACE": "1"}
        if provider == "native":
            bad = run([executable], cwd=cwd, env=activation)
            require(bad.returncode > 0 and not bad.stdout and "policy" in bad.stderr.lower()
                    and "kernel provider" not in bad.stderr, "native without policy", bad)
            activation["ON1X_KERNEL_NATIVE_POLICY"] = str(args.policy.resolve(strict=True))
        trace = (f"on1x: kernel provider module=stdffi.clamp provider={provider} "
                 f"package_sha256={digest}\n")
        result = run([executable], cwd=cwd, env=activation)
        check_values(provider, result, vectors, expected)
        require(result.stderr == trace, f"{provider} and exact package identity", result)
        bad = run([guard_executable], cwd=cwd, env=activation)
        require(bad.returncode == 70 and bad.stdout == "1\n3\n2\n"
                and trace in bad.stderr and "guard" in bad.stderr.lower(),
                f"{provider} invalid interval", bad)
        if sha256(executable.read_bytes()).hexdigest() != binary_digest:
            raise RuntimeError(f"{provider} modified the already-linked executable")
        print(f"PASS load-time {provider}: {len(vectors)} oracle cases, "
              f"ordered guard rejection, package SHA-256 {digest}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path, help="fresh working directory (must not exist)")
    parser.add_argument("--baseline", type=Path, metavar="ON1XC",
                        help="also verify the build-pinned MIR evaluator and VM")
    parser.add_argument("--baseline-guard", choices=("observe", "reject", "unchecked"),
                        default="observe", help="expected handling of out-of-contract legacy calls")
    parser.add_argument("--recipe", type=Path, help="prebuilt recipe-only clamp package")
    parser.add_argument("--native", type=Path, help="prebuilt native clamp package")
    parser.add_argument("--policy", type=Path, help="trusted host policy for --native")
    tools = parser.add_mutually_exclusive_group()
    tools.add_argument("--install-build", type=Path,
                       help="install and relocate from this fully built CMake tree")
    tools.add_argument("--compiler", type=Path, help="load-time-capable on1xc")
    parser.add_argument("--builder", type=Path, help="on1x-build.py for --compiler")
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--cflag", action="append", default=[], help="extra host C compiler flag")
    args = parser.parse_args()
    if bool(args.native) != bool(args.policy):
        parser.error("--native and --policy must be provided together")
    if bool(args.compiler) != bool(args.builder):
        parser.error("--compiler and --builder must be provided together")
    load_time = args.recipe or args.native
    if bool(load_time) != bool(args.install_build or args.compiler):
        parser.error("provider qualification needs package(s) and load-time tools")
    work = args.work.resolve()
    work.mkdir()
    source, guard, vectors, expected = write_programs(work)
    if args.baseline:
        compiler = args.baseline.resolve(strict=True)
        for mode in ("--run-ir", "--run-vm"):
            result = run([compiler, mode, source], cwd=work, env=clean_env())
            check_values(mode, result, vectors, expected)
            bad = run([compiler, mode, guard], cwd=work, env=clean_env())
            if bad.returncode == 70 and bad.stdout == "1\n3\n2\n":
                observed = "reject"
            elif bad.returncode == 0 and bad.stdout == "1\n3\n2\n3\n17001\n":
                observed = "unchecked"
            else:
                require(False, f"{mode} unexpected invalid interval outcome", bad)
            require(args.baseline_guard in ("observe", observed), f"{mode} legacy guard", bad)
            print(f"PASS {mode}: {len(vectors)} in-contract oracle cases; "
                  f"out-of-contract interval: {observed} (exit {bad.returncode})")
    if load_time:
        qualify_load_time(args, work, source, guard, vectors, expected)
    elif not args.baseline:
        print(f"Generated {len(vectors)} independent oracle cases under {work}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as exc:
        sys.exit(str(exc))
