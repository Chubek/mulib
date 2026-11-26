#!/usr/bin/env python3
"""Compile and run the socket-free On1x probe, directly and after durable dispatch."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ON1X = Path(__file__).resolve().parents[1]
EXPECTED_OUTPUT = "fixture title: Daily & notes\nmanual fixture: Open studio\n"


def check(compiler, cc, stdlib, runtime, kernels, example, cflags, work, package_tests):
    # Exercise the selected stdlib, not caller-configured search paths, plugins
    # or a globally configured TMPDIR. No ambient inputs are fixture authority.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("ON1X_", "MULIB_"))}
    env["TMPDIR"] = str(work)

    def run(*args):
        return subprocess.run(args, cwd=work, env=env, check=True, text=True,
                              stdout=subprocess.PIPE, timeout=75).stdout

    compile_c = [cc, "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-Wall", "-Wextra",
                 "-Wpedantic", "-Werror", *cflags, "-UNDEBUG",
                 "-I", kernels, "-I", runtime, "-I", stdlib]
    libs = ["-pthread", "-ldl", "-lm"]
    compiled = {}

    def object_file(source, *flags):
        key = (source, flags)
        if key not in compiled:
            obj = work / f"native-{len(compiled)}.o"
            run(*compile_c, *flags, "-c", source, "-o", obj)
            compiled[key] = obj
        return compiled[key]

    def emit(source, name):
        generated = work / f"{name}.c"
        run(compiler, "--emit-c", "--stdlib", stdlib, source, "-o", generated)
        return generated

    def link(name, objects):
        executable = work / name
        run(*compile_c, *objects, *libs, "-o", executable)
        return executable

    def expect_output(name, result, expected):
        if result != expected:
            raise AssertionError(f"{name}: unexpected output {result!r}; expected {expected!r}")

    generated = emit(example / "main.on1x", "probe")
    rt = object_file(runtime / "on1x_runtime.c")
    elvi = [object_file(stdlib / f"elvi/{name}.c") for name in ("runtime", "bridge", "capture")]
    html = object_file(stdlib / "html/runtime.c")
    kernel_impl = object_file(example / "kernels.c")
    common = [rt, *elvi, html, kernel_impl]
    live = object_file(stdlib / "elvi/live.c")
    native_fixture = object_file(example / "fixture.c")
    example_libs = [*common, live, native_fixture]
    expect_output("one-shot", run(link("probe", [generated, *example_libs])), EXPECTED_OUTPUT)

    native = [object_file(ON1X / "tests/fixtures/http_probe_schedule.c"),
              object_file(stdlib / "http/probe/context.c"),
              object_file(stdlib / "http/probe/supervised.c"),
              object_file(stdlib / "kron/runtime.c"),
              object_file(stdlib / "daemon/supervisor.c")]
    cache = work / "cache"
    cache.mkdir(mode=0o700)
    app = object_file(generated, "-Dmain=probe_main")
    scheduled = link("scheduled", [app, *example_libs, *native])
    # Daemon redirects worker stdout to /dev/null; the parent verifies the
    # worker's result via its checked exit code, not inherited stdio.
    expect_output("scheduled", run(scheduled, cache), "durable fixture: one due run, two denials\n")

    if not package_tests:
        return
    missing_host = subprocess.run(
        [*compile_c, generated, *common, live, *libs, "-o", work / "missing-host"],
        cwd=work, env=env, capture_output=True, text=True, timeout=75)
    if missing_host.returncode == 0 or "on1x_elvi_live_authorize_v1" not in missing_host.stderr:
        raise AssertionError(f"missing mandatory Elvi authorizer was not diagnosed: {missing_host}")

    checks = stdlib / "http/probe/tests"
    live_host = object_file(stdlib / "elvi/tests/live_host.c", "-I", stdlib / "elvi")
    for name, source, objects, expected in (
        ("probe-spec", checks / "spec.on1x", common, ""),
        ("elvi-document-values", stdlib / "tests/elvi_capture.on1x", [rt],
         "Elvi document values: OK\n"),
        ("elvi-capture-operation", stdlib / "tests/elvi_capture_operation.on1x", common,
         "Elvi capture operation: OK\n"),
        ("kron-types", stdlib / "kron/tests/schedule.on1x", [rt], ""),
        ("daemon-types", stdlib / "daemon/fixtures/lifecycle_types.on1x", [rt],
         "fixture\n42\nfalse\n"),
        ("elvi-live", stdlib / "elvi/tests/live.on1x",
         [rt, *elvi, live, object_file(stdlib / "elvi/kernels.c"), live_host],
         "Elvi live bridge: ok\n"),
        ("probe-artifact", checks / "artifact.on1x",
         [rt, html, object_file(stdlib / "http/probe/artifact.c"),
          # A single implementation TU combines HTML with the artifact host's
          # workspace-fs implementation. It does not link kernel_impl above.
          object_file(checks / "artifact_host.c", "-I", stdlib / "http/probe",
                      "-include", stdlib / "html/kernels.c")],
         "Probe artifact: ok\n"),
    ):
        expect_output(name, run(link(name, [emit(source, name), *objects])), expected)

    for source, diagnostic in (
        (checks / "reject_parser_input.on1x", "nested mismatch"),
        (checks / "reject_parser_result.on1x", "nested mismatch"),
        (checks / "reject_handler_result.on1x", "nested mismatch"),
        (ON1X / "tests/fixtures/http_probe_reject_borrow.on1x", "cannot capture borrowed place"),
    ):
        rejected = subprocess.run([compiler, "--check", "--stdlib", stdlib, source],
                                  cwd=work, env=env, capture_output=True, text=True, timeout=30)
        if rejected.returncode != 1 or diagnostic not in rejected.stderr:
            raise AssertionError(f"expected {source.name} to reject {diagnostic!r}: {rejected}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--on1xc", type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--stdlib", type=Path, default=ON1X / "stdlib")
    parser.add_argument("--runtime", type=Path, default=ON1X / "runtime")
    parser.add_argument("--kernels", type=Path, default=ON1X.parent / "kernels")
    parser.add_argument("--example", type=Path, default=ON1X / "examples/http-probe")
    parser.add_argument("--scratch", type=Path, default=Path.cwd())
    parser.add_argument("--cflag", action="append", default=[])
    parser.add_argument("--package-tests", action="store_true", help="also run source-tree package tests")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="on1x-http-probe-", dir=args.scratch.resolve()) as temp:
        check(args.on1xc.resolve(), args.cc, args.stdlib.resolve(), args.runtime.resolve(),
              args.kernels.resolve(), args.example.resolve(), args.cflag, Path(temp),
              args.package_tests)


if __name__ == "__main__":
    main()
