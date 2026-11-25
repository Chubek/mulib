#!/usr/bin/env python3
"""Build and compare two independently produced load-time clamp packages."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


HERE = Path(__file__).resolve().parent
MODULE = "stdffi.clamp"
PACKAGE = MODULE + ".on1xkp"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(*command, status=0, **kwargs):
    proc = subprocess.run(command, capture_output=True, timeout=120, **kwargs)
    if proc.returncode != status:
        raise ValueError(
            f"{command[0]}: expected exit {status}, got {proc.returncode}\n"
            + proc.stderr.decode(errors="replace")
        )
    return proc


def native_policy(info):
    # Demonstration only: the operator trusts the producer and the specific
    # image built below. In deployment, review before authorizing an image.
    rows = [
        "on1x-native-policy-v1",
        f"x86_64-unknown-linux-gnu sysv-clamp-v1 {MODULE} 1 {info['image_sha256']}",
    ]
    for export in sorted(info["exports"], key=lambda entry: entry["name"]):
        name = export["name"]
        rows.append(
            f"{name} {export['arity']} on1x_kernel_native_{name} "
            f"{export['interface_sha256']} {export['behavior_sha256']}"
        )
    return "\n".join(rows) + "\n"


def demo(args):
    work = args.work_dir.resolve()
    if ":" in str(work):
        raise ValueError("work directory must not contain ':' (the package-root separator)")
    work.mkdir()
    deploy = work / "deploy"
    deploy.mkdir()
    cwd = work / "empty-cwd"
    cwd.mkdir()

    # Build from the installed SDK, before either package even exists.
    prefix = args.prefix.resolve()
    build = prefix / "share/on1x/tools/on1x-build.py"
    app = deploy / "clamp"
    run(sys.executable, "-B", build, HERE / "main.on1x", "--kernel-load-time",
        "--on1xc", prefix / "bin/on1xc", "--cc", args.cc, "-o", app)
    app_sha = sha256(app)
    print(f"application sha256={app_sha} {app}")

    oracle = work / "oracle"
    run(args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
        HERE / "oracle.c", "-o", oracle)
    expected = run(oracle)
    expected_guard = run(oracle, "guard", status=70)
    (work / "oracle.stdout").write_bytes(expected.stdout)

    tool = prefix / "bin/on1x-kernel-package"
    cephyr = args.cephyr_prefix.resolve()
    roots = {kind: deploy / kind for kind in ("portable", "native")}
    packages = {}
    for kind, root in roots.items():
        root.mkdir()
        package = root / PACKAGE
        run(sys.executable, "-B", tool, "build", "--kind", kind,
            "--header", args.header.resolve(),
            "--cephyr-library", cephyr / "lib/libcephyr.so",
            "--cephyr-resource-root", cephyr / "share/cephyr",
            "--cc", args.cc, "--output", package)
        report = run(sys.executable, "-B", tool, "inspect", package).stdout
        (work / f"{kind}.json").write_bytes(report)
        info = json.loads(report)
        if (info["module"], info["version"], info["kind"]) != (MODULE, 1, kind):
            raise ValueError(f"{package}: unexpected module, version, or kind")
        if info["package_sha256"] != sha256(package):
            raise ValueError(f"{package}: inspector package digest mismatch")
        packages[kind] = info
    portable, native = packages["portable"], packages["native"]
    if portable["package_sha256"] == native["package_sha256"]:
        raise ValueError("expected two distinct package artifacts")
    if portable["exports"] != native["exports"]:
        raise ValueError("package interface or behavior digests differ")
    if portable["image_sha256"] is not None or not native["image_sha256"]:
        raise ValueError("expected image digest only for native package")
    policy = deploy / "native.policy"
    policy.write_text(native_policy(native), encoding="ascii")
    print(f"native image sha256={native['image_sha256']}")

    # A package in CWD doesn't make it a root, and an image digest stored in
    # a package isn't permission to execute it.
    for label, directory, env, diagnostic in (
        ("no-root", roots["portable"], {}, b"no explicit package roots"),
        ("no-policy", cwd, {"ON1X_KERNEL_PACKAGE_ROOTS": str(roots["native"])},
         b"native package requires explicit ON1X_KERNEL_NATIVE_POLICY"),
    ):
        denied = run(app, cwd=directory, env=env, status=1)
        (work / f"{label}.stderr").write_bytes(denied.stderr)
        if denied.stdout or diagnostic not in denied.stderr:
            raise ValueError(f"{label}: unexpected startup result: {denied.stderr!r}")

    for kind, root in roots.items():
        provider = "recipe" if kind == "portable" else "native"
        provider_id = packages[kind]["package_sha256"]
        trace = (
            f"on1x: kernel provider module={MODULE} provider={provider} "
            f"package_sha256={provider_id}"
        ).encode("ascii")
        # Deliberately pass no ambient PATH, CWD dependency, source search
        # paths, or native policy in the portable case.
        env = {"ON1X_KERNEL_PACKAGE_ROOTS": str(root), "ON1X_KERNEL_TRACE": "1"}
        if kind == "native":
            env["ON1X_KERNEL_NATIVE_POLICY"] = str(policy)
        result = run(app, cwd=cwd, env=env)
        (work / f"{kind}.stdout").write_bytes(result.stdout)
        (work / f"{kind}.stderr").write_bytes(result.stderr)
        if result.stdout != expected.stdout:
            raise ValueError(f"{kind}: application stdout differs from the C oracle")
        if result.stderr.splitlines() != [trace]:
            raise ValueError(f"{kind}: unexpected provider trace: {result.stderr!r}")
        rejected = run(app, "guard", cwd=cwd, env=env, status=expected_guard.returncode)
        (work / f"{kind}-guard.stderr").write_bytes(rejected.stderr)
        guard_diagnostic = b"on1x: kernel import stdffi.clamp/i64: checked kernel guard rejected arguments"
        if (rejected.stdout != expected_guard.stdout
                or rejected.stderr.splitlines() != [trace, guard_diagnostic]):
            raise ValueError(f"{kind}: unexpected guard result: {rejected.stderr!r}")
        if sha256(app) != app_sha:
            raise ValueError(f"{kind}: launched application digest changed")
        print(trace.decode("ascii"))
    print("Both providers matched the C oracle (26 results; reversed bounds rejected).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path,
                        help="installed On1x SDK prefix")
    parser.add_argument("--cephyr-prefix", required=True, type=Path,
                        help="installed Cephyr prefix with lib/libcephyr.so and share/cephyr")
    parser.add_argument("--header", required=True, type=Path,
                        help="canonical MuLIB kernels/mulib-clamp.h")
    parser.add_argument("--cc", default="cc", help="host C compiler (GCC or Clang)")
    parser.add_argument("--work-dir", required=True, type=Path,
                        help="fresh output directory (must not exist)")
    parser.add_argument("--allow-demo-native", required=True, action="store_true",
                        help="authorize in-process execution of the demo's generated native image")
    args = parser.parse_args()
    try:
        demo(args)
    except (ValueError, KeyError, OSError, subprocess.TimeoutExpired) as exc:
        parser.exit(1, f"{parser.prog}: {exc}\n")


if __name__ == "__main__":
    main()
