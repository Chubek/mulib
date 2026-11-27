#!/usr/bin/env python3
"""Exercise both recipe inputs through CMake's build and install rules."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile

cmake, generator, make, cxx, handler, work, library = sys.argv[1:]
handler = Path(handler)
reviewed = json.loads((handler / "reviewed.json").read_text(encoding="utf-8"))
recipe_names = {f"{kernel}.ffi" for kernel in reviewed}


def run(*args, error=None):
    result = subprocess.run([cmake, *args], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    if error is None:
        assert result.returncode == 0, result.stdout
    else:
        assert result.returncode != 0 and error in result.stdout, result.stdout


with tempfile.TemporaryDirectory(prefix="contract cmake ", dir=work) as path:
    root = Path(path)
    source = root / "fixture"
    source.mkdir()
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.20)\n"
        "project(On1x LANGUAGES CXX)\n"
        "add_library(on1x_frontend STATIC EXCLUDE_FROM_ALL)\n"
        'add_subdirectory("${FFIHANDLER_SOURCE}" ffi)\n',
        encoding="utf-8",
    )

    def configure(build, *options, error=None):
        run("-S", source, "-B", build, "-G", generator,
            f"-DCMAKE_MAKE_PROGRAM={make}", f"-DCMAKE_CXX_COMPILER={cxx}",
            "-DBUILD_TESTING=OFF", "-DCMAKE_DISABLE_FIND_PACKAGE_Perl=ON",
            f"-DFFIHANDLER_SOURCE={handler}", *options, error=error)

    def build_tables(build, *, error=None):
        run("--build", build, "--target", "on1x_stdffi_tables", error=error)

    def stage(build, name="clamp.ffi"):
        return build / "stdffi/mulib" / name

    def generated(build, name="clamp.ffi"):
        return build / "ffi/generated/recipes" / name

    def check_install(build, expected):
        prefix = root / f"installed-{build.name}"
        run("--install", build, "--prefix", prefix)
        installed = prefix / "share/on1x/stdffi/mulib"
        assert {spec.name: spec.read_bytes() for spec in installed.iterdir()} == expected

    if not list((handler / "../../stdffi/mulib").glob("*.ffi")):
        configure(root / "no-input", error="No executable stdffi recipes")

    build = root / "from-contracts"
    configure(build, f"-DON1X_STDFFI_CEPHYR_LIBRARY={library}")
    recipe = generated(build)
    assert not recipe.exists() and not stage(build).exists()
    build_tables(build)
    expected = {spec.name: spec.read_bytes() for spec in recipe.parent.iterdir()}
    assert expected.keys() == recipe_names
    for name, document in expected.items():
        assert document.startswith(f"(ffispec 1 (kernel {Path(name).stem})".encode())
        assert stage(build, name).read_bytes() == document
    timestamp = recipe.stat().st_mtime_ns
    build_tables(build)
    assert recipe.stat().st_mtime_ns == timestamp
    recipe.unlink()
    stage(build).unlink()
    build_tables(build)
    assert recipe.read_bytes() == stage(build).read_bytes() == expected["clamp.ffi"]
    check_install(build, expected)

    external = root / "external/clamp.ffi"
    external.parent.mkdir()
    for name, document in expected.items():
        external.with_name(name).write_bytes(document)
    overrides = ";".join(str(external.with_name(name)) for name in sorted(expected))
    override = root / "from-override"
    configure(override, f"-DON1X_STDFFI_SPEC_FILES={overrides}",
              f"-DON1X_STDFFI_CEPHYR_LIBRARY={root / 'missing-cephyr'}")
    build_tables(override)
    assert not generated(override).exists()
    for name, document in expected.items():
        assert stage(override, name).read_bytes() == document
    check_install(override, expected)
    external.write_bytes(expected["clamp.ffi"] + b"\n")
    build_tables(override, error="stale FFIHandler bootstrap")
    assert stage(override).read_bytes() == expected["clamp.ffi"]
