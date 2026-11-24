#!/usr/bin/env python3
"""Separate unprivileged gates for the fixed-fixture Linux Pod adapter."""

import argparse
import ctypes
import errno
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile


def run(*args, cwd=None, error=None, allowed=(0,)):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=45)
    if error is not None:
        assert result.returncode != 0 and error in result.stderr, result
    else:
        assert result.returncode in allowed, result
    return result


def skip(message):
    print(f"SKIP Pod: {message}")
    return 77


def native(args, root, work, cgroup):
    container = root / "stdlib/container"
    kernel = root.parent / "kernels"
    cc = [
        args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-I", kernel, "-I", container,
    ]
    worker = work / "worker"
    run(*cc, "-static", "-s", container / "pod_worker.c", "-o", worker)
    data = worker.read_bytes()
    embed = work / "worker_image.c"
    with embed.open("w", encoding="ascii") as file:
        file.write("#include <stddef.h>\nconst unsigned char on1x_container_pod_worker_elf[] = {\n")
        for i in range(0, len(data), 32):
            file.write(",".join(str(b) for b in data[i:i + 32]) + ",\n")
        file.write("};\nconst size_t on1x_container_pod_worker_elf_size = "
                   "sizeof on1x_container_pod_worker_elf;\n")
    kernels = work / "kernels.c"
    kernels.write_text('#define _GNU_SOURCE\n#include "pod_kernel.h"\n', encoding="ascii")
    native_path = work / "pod"
    wrap = ("mulib_sandbox_process_start", "mulib_sandbox_process_send",
            "mulib_sandbox_process_receive", "mulib_sandbox_process_stop",
            "mulib_cgroup2_attach", "unshare")
    run(*cc, *args.cflag, "-DON1X_CONTAINER_POD_EMBEDDED",
        root / "tests/test_container_pod.c", container / "pod_runtime.c", kernels, embed,
        "-pthread", *(f"-Wl,--wrap={sym}" for sym in wrap), "-o", native_path)
    command = [native_path]
    if args.mode == "namespace":
        command.append("namespaces")
    if args.mode == "cgroup":
        command += ["cgroup", cgroup]
    result = run(*command, cwd=work, allowed=(0, 77))
    print(result.stdout, end="")
    return result.returncode


def on1x(args, root, work):
    container = root / "stdlib/container"
    fixture = root / "tests/fixtures/container_pod.on1x"
    compiler = args.on1xc.resolve()
    driver = [
        sys.executable, root / "tools/on1x-build.py",
        "--on1xc", compiler, "--cc", args.cc, *(f"--cflag={flag}" for flag in args.cflag),
    ]
    program = work / "pod-program"
    plan = work / "link.json"
    run(*driver, args.worker_option, fixture, "-o", program, "--mulib-plan", plan)
    assert run(program, cwd=work).stdout == "On1x Pod echo ok\n"
    report = json.loads(plan.read_text())
    assert report["container_bridge_groups"] == ["POD"] and report["pod_fixture_enabled"] is True
    assert report["safe_export"] is False and report["plan"]["implementation_translation_units"] == 1
    for name in ("pod_runtime.c", "pod_runtime.h", "pod_worker.c", "pod_kernel.h"):
        assert str(container / name) in report["inputs"]
    assert "pod-worker.elf" in report["generated_inputs"]
    generated = work / "generated.c"
    run(compiler, "--emit-c", fixture, "-o", generated)
    run(args.cc, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
        "-I", root / "runtime", "-include", container / "pod_runtime.h", "-fsyntax-only", generated)
    denied = work / "denied.on1x"
    denied.write_text("""\
import std.container.Pod as Pod;
fn main() {
  match Pod.basis_sandbox_echo_byte(1u8, 2000) with {
    Ok(_) => assert(false), Err(error) => match error with {
      PodNotAuthorized() => assert(true), _ => assert(false)
    }
  };
}
""", encoding="ascii")
    run(*driver, denied, "-o", program, "--mulib-plan", plan)
    run(program, cwd=work)
    report = json.loads(plan.read_text())
    assert report["pod_fixture_enabled"] is False and report["plan"] is None
    assert "pod-worker.elf" not in report["generated_inputs"]
    for filename, expression, error in (
        ("not_a_byte", "Pod.basis_sandbox_echo_byte(\"executable\", 2000);", "expected u8, got string"),
        ("not_a_namespace", "Pod.basis_pod_echo_byte(1u8, 2000, 2, true);", "got i64"),
        ("no_thread", "taskgroup { let child = launch { Pod.basis_sandbox_echo_byte(1u8, 2000); 0 }; join child; }",
         "raw foreign effects"),
    ):
        source = work / f"{filename}.on1x"
        source.write_text("import std.container.Pod as Pod;\nfn main() { " + expression + " }\n",
                          encoding="ascii")
        run(compiler, "--check", source, error=error)
    run(compiler, "--kernel-c11", "--emit-c", fixture, "-o", work / "kernel.c",
        error="ON1X-KERNEL-EXTERN")
    run(compiler, "--emit-ir", fixture, "-o", work / "mir.json", error="ON1X-MIR-UNSUPPORTED")
    assert not (work / "kernel.c").exists() and not (work / "mir.json").exists()
    print("On1x Pod: generated C11, checked ABI, explicit host opt-in and frontend rejection")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["native", "namespace", "cgroup", "on1x"])
    parser.add_argument("cc")
    parser.add_argument("work", type=Path)
    parser.add_argument("--on1xc", type=Path)
    parser.add_argument("--worker-option", default="--enable-pod-fixture")
    parser.add_argument("--cflag", action="append", default=[])
    args = parser.parse_args()
    if args.mode == "on1x" and args.on1xc is None:
        parser.error("on1x mode requires --on1xc")
    target = run(args.cc, "-dumpmachine").stdout.split("-", 1)[0]
    if (sys.platform != "linux" or platform.machine() != target or
            target not in ("x86_64", "aarch64") or ctypes.sizeof(ctypes.c_void_p) != 8 or
            sys.byteorder != "little" or not Path("/proc/self/fd").is_dir()):
        return skip("requires native LP64 little-endian Linux x86-64/AArch64 with procfs")
    if platform.libc_ver()[0] != "glibc":
        return skip("test profile requires glibc")
    if not os.getuid() or not os.getgid() or os.getuid() != os.geteuid() or os.getgid() != os.getegid():
        return skip("requires a non-root, non-set-ID supervisor")
    for line in Path("/proc/self/status").read_text(encoding="ascii").splitlines():
        name, _, value = line.partition(":")
        if name in {"CapPrm", "CapEff", "CapAmb"} and int(value, 16):
            return skip("requires an unprivileged supervisor")
    if not hasattr(os, "memfd_create"):
        return skip("test interpreter lacks os.memfd_create")
    try:
        fd = os.memfd_create("on1x-pod-test-prerequisite", 0x0010 | os.MFD_ALLOW_SEALING | os.MFD_CLOEXEC)
    except PermissionError:
        return skip("kernel policy prohibits MFD_EXEC")
    except OSError as error:
        # Only missing capability is skippable: unexpected I/O/resource errors fail.
        if error.errno in (errno.EINVAL, errno.ENOSYS):
            return skip("kernel lacks MFD_EXEC")
        raise
    os.close(fd)
    cgroup = os.environ.get("ON1X_TEST_POD_CGROUP_PARENT")
    if args.mode == "cgroup" and not cgroup:
        return skip("no exclusive delegated cgroup v2 parent (ON1X_TEST_POD_CGROUP_PARENT)")

    args.work.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix=f"pod-{args.mode}-", dir=args.work.resolve()) as dirname:
        work = Path(dirname)
        if args.mode == "on1x":
            return on1x(args, root, work)
        return native(args, root, work, cgroup)


if __name__ == "__main__":
    sys.exit(main())
