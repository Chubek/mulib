#!/usr/bin/env python3
"""Generate, build, test and install the canonical AMD64 SysV Archon backend."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--libstone-root", type=Path,
                        default=Path(__file__).resolve().parents[2] / "third_party/libstone")
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    root = args.libstone_root.resolve()
    archon = root / "cornerstone/Archon"
    if not (archon / "stdspec/amd64/root.arch").is_file():
        parser.error(f"canonical AMD64 specification unavailable under {archon}")
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    prefix = work / "install"
    temporary = work / "tmp"
    temporary.mkdir(exist_ok=True)
    environment = dict(os.environ, TMPDIR=str(temporary))

    def run(*command):
        print("+", " ".join(map(str, command)), flush=True)
        subprocess.run(list(map(str, command)), cwd=archon, env=environment, check=True)

    run(sys.executable, "-m", "archon", "compile", "stdspec/amd64/root.arch",
        "-o", work / "amd64.archdb")
    run(sys.executable, "-m", "archon", "irgen", "gated",
        "--archdb", work / "amd64.archdb", "--profile", "native",
        "--abi", "SysVAMD64", "--no-oracle", "-o", work / "generated")
    run("cmake", "-S", work / "generated", "-B", work / "build",
        "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}")
    run("cmake", "--build", work / "build", "--parallel", args.jobs)
    run(sys.executable, "-m", "unittest", "discover", "-s", "tests",
        "-p", "test_native_scalar_gas.py", "-v")
    run("cmake", "--install", work / "build")
    revision = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    (work / "on1x-toolchain.json").write_text(json.dumps({
        "libstone_revision": revision, "libstone_root": str(root),
        "target": "x86_64-unknown-linux-gnu", "abi": "SysVAMD64",
        "prefix": str(prefix),
    }, indent=2) + "\n", encoding="utf-8")
    print(f"On1x configure: -DCMAKE_PREFIX_PATH={prefix} "
          "-DON1X_BUILD_MIR_ARCHON=ON -DON1X_ARCHON_ENABLE_SDK_ADAPTER=ON "
          "-DON1X_ARCHON_TARGET_TRIPLE=x86_64-unknown-linux-gnu", flush=True)


if __name__ == "__main__":
    main()
