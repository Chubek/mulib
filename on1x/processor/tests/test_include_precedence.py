#!/usr/bin/env python3
"""Ensure the production processor CMake selects its own generated header."""
import argparse
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
    manifest = json.loads((processor / "generated/rules_manifest.json").read_text())
    # Reproduce the original integration failure: the parent adds its binary
    # directory before adding the real processor subdirectory.
    stale = build / "on1x_meta_rules.hpp"
    stale.write_text('#error "stale parent header shadowed real On1xMETA tables"\n')
    (source / "probe.cpp").write_text(
        "#include <on1x_meta_rules.hpp>\n"
        f"static_assert(on1x::meta::generated::rules.size() == {manifest['rules']});\n"
        f"static_assert(on1x::meta::generated::constraints.size() == {manifest['constraints']});\n"
        "static_assert(sizeof(on1x::meta::generated::program_sha256) == 65);\n")
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(On1xMetaIncludeRegression LANGUAGES CXX)\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(BUILD_TESTING OFF)\n"
        "add_library(on1x_frontend STATIC probe.cpp)\n"
        'target_include_directories(on1x_frontend PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")\n'
        f'target_include_directories(on1x_frontend PRIVATE "{processor.parent / "include"}")\n'
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
    print("Production CMake ignores stale parent header and compiles real generated tables: PASS")


if __name__ == "__main__":
    main()
