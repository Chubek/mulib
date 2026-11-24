#!/usr/bin/env python3
"""Ensure the production processor CMake selects its own generated header."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--processor", type=Path, required=True)
    cli.add_argument("--work", type=Path, required=True)
    cli.add_argument("--libstone", type=Path, required=True)
    cli.add_argument("--cmake", required=True)
    cli.add_argument("--cxx", required=True)
    args = cli.parse_args()
    processor, work = args.processor.resolve(), args.work.resolve()
    source, build = work / "source", work / "build"
    source.mkdir(parents=True, exist_ok=True)
    build.mkdir(parents=True, exist_ok=True)
    # Reproduce the original integration failure: the parent adds its binary
    # directory before adding the real processor subdirectory.
    stale = build / "on1x_meta_rules.hpp"
    stale.write_text('#error "stale parent header shadowed real On1xMETA tables"\n')
    (source / "probe.cpp").write_text(
        "#include <on1x_meta_rules.hpp>\n"
        "#include <iostream>\n"
        "int main() { namespace g = on1x::meta::generated;\n"
        'std::cout << g::program_sha256 << " " << g::rules.size() << " "'
        ' << g::constraints.size() << "\\n"; }\n')
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(On1xMetaIncludeRegression LANGUAGES CXX)\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(BUILD_TESTING OFF)\n"
        "add_executable(on1x_frontend probe.cpp)\n"
        'target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")\n'
        f'target_include_directories(on1x_frontend PRIVATE "{processor.parents[1] / "include"}")\n'
        f'add_subdirectory("{processor}" processor)\n')
    for command in [
        [args.cmake, "-S", str(source), "-B", str(build),
         "-DCMAKE_CXX_COMPILER=" + args.cxx,
         "-DON1X_LIBSTONE_ROOT=" + str(args.libstone.resolve())],
        [args.cmake, "--build", str(build), "--target", "on1x_frontend", "--parallel", "2"],
    ]:
        result = subprocess.run(command, text=True, capture_output=True, timeout=120)
        assert result.returncode == 0, (command, result.stdout, result.stderr)
    assert stale.read_text() == '#error "stale parent header shadowed real On1xMETA tables"\n'
    # Source checkouts need no generated snapshots. Verify the actual nested
    # production build consumed *all* shipping modules, then compare the
    # linked probe's table identity/counts against that build's manifest.
    manifest = json.loads((build / "processor/generated/rules_manifest.json").read_text())
    expected = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                for p in processor.glob("*.o1m")}
    assert expected and {s["file"]: s["sha256"] for s in manifest["sources"]} == expected
    assert manifest["rules"] > 0 and manifest["constraints"] > 0, manifest
    probe = subprocess.run([build / "on1x_frontend"], capture_output=True,
                           text=True, timeout=10, check=True)
    assert probe.stdout.split() == [manifest["program_sha256"], str(manifest["rules"]),
                                    str(manifest["constraints"])], probe.stdout
    print("Production CMake ignores stale parent header and compiles real generated tables: PASS")


if __name__ == "__main__":
    main()
