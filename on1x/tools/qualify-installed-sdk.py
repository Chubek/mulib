"""Install or copy an On1x prefix, move it, and test installed consumers only."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def child_path(value):
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise argparse.ArgumentTypeError("expected a relative install directory")
    return path


def verify(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--build", type=Path, help="completed On1x build to install")
    source.add_argument("--copy-installed", type=Path,
                        help="copy an existing installation; not a fresh-install check")
    parser.add_argument("--work", required=True, type=Path, help="new evidence directory")
    parser.add_argument("--config", default="Release")
    parser.add_argument("--libdir", default=Path("lib"), type=child_path)
    parser.add_argument("--datadir", default=Path("share"), type=child_path)
    parser.add_argument("--cmake", type=Path, default=Path("/usr/bin/cmake"))
    parser.add_argument("--cc", type=Path, default=Path("/usr/bin/cc"))
    parser.add_argument("--cxx", type=Path, default=Path("/usr/bin/c++"))
    parser.add_argument("--require-qmr", action="store_true")
    args = parser.parse_args()
    input_root = (args.build or args.copy_installed).resolve(strict=True)
    work = args.work.resolve()
    verify(input_root.is_dir(), "build or installed input must be a directory")
    verify(not work.is_relative_to(input_root), "work must be outside the input directory")
    work.mkdir()
    for name in ("tmp", "home", "consumer", "project"):
        (work / name).mkdir()
    prefix = work / "original"
    moved = work / "moved prefix"
    env = {
        "HOME": str(work / "home"), "PATH": os.defpath, "TMPDIR": str(work / "tmp"),
        "LC_ALL": "C", "PYTHONDONTWRITEBYTECODE": "1",
    }

    def run(name, *cmd, cwd=work, environment=env, returncode=0, input_text=""):
        argv = [str(word) for word in cmd]
        print(f"{name}: {shlex.join(argv)}", flush=True)
        result = subprocess.run(argv, cwd=cwd, env=environment, input=input_text, text=True,
                                encoding="utf-8", errors="replace",
                                capture_output=True, timeout=120)
        (work / f"{name}.stdout").write_text(result.stdout, encoding="utf-8")
        (work / f"{name}.stderr").write_text(result.stderr, encoding="utf-8")
        verify(result.returncode == returncode,
               f"{name}: exit {result.returncode}, expected {returncode}; see {work}")
        return result.stdout

    if args.build:
        run("install", args.cmake, "--install", input_root,
            "--config", args.config, "--prefix", prefix)
    else:
        print(f"Copy-only input: {input_root}", flush=True)
        shutil.copytree(input_root, prefix, symlinks=True)
    prefix.rename(moved)
    verify(not prefix.exists(), "original prefix still exists after move")

    # An installed consumer gets no checkout include dirs, registries, or
    # user-configured package search paths.
    consumer = work / "consumer"
    source_dir = moved / args.libdir / "cmake/On1xSDK"
    verify((source_dir / "On1xSDKConfig.cmake").is_file(), "SDK CMake package missing")
    (consumer / "CMakeLists.txt").write_text("""\
cmake_minimum_required(VERSION 3.20)
project(MovedOn1x LANGUAGES CXX)
find_package(On1xSDK 2 CONFIG REQUIRED PATHS "${SDK_PACKAGE}" NO_DEFAULT_PATH)
if(NOT On1xSDK_PROJECT_ABI_VERSION EQUAL 2)
  message(FATAL_ERROR "unexpected project ABI")
endif()
add_executable(consumer main.cpp)
set_target_properties(consumer PROPERTIES CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(consumer PRIVATE On1x::Project)
enable_testing()
add_test(NAME installed_consumer COMMAND consumer)
""", encoding="utf-8")
    (consumer / "main.cpp").write_text(r"""#include <On1xSDK.hpp>
int main() {
    auto manifest = on1x::sdk::Manifest::parse(
        "on1xfile 1\nkind executable\nname \"moved\"\n"
        "target \"moved\" \"main.on1x\"\n");
    return manifest.name() == "moved" && manifest.target_count() == 1 ? 0 : 1;
}
""", encoding="utf-8")
    consumer_build = work / "consumer-build"
    run("consumer-configure", args.cmake, "-S", consumer, "-B", consumer_build,
        "-G", "Unix Makefiles", f"-DSDK_PACKAGE={source_dir}",
        f"-DCMAKE_CXX_COMPILER={args.cxx}", f"-DCMAKE_BUILD_TYPE={args.config}")
    run("consumer-build", args.cmake, "--build", consumer_build, "--parallel", "2")
    run("consumer-run", args.cmake.with_name("ctest"), "--test-dir", consumer_build,
        "--output-on-failure")

    sdk = moved / "bin/on1x-sdk"
    compiler = moved / "bin/on1xc"
    installed = moved / args.datadir / "on1x"
    info = json.loads(run("sdk-info", sdk, "info"))
    verify(info.get("schema") == "on1x.sdk.info/1" and info.get("project_abi") == 2,
           "unexpected SDK metadata")
    binding = moved / args.libdir / "on1x-sdk/python"
    run("sdk-python", sys.executable, "-IB", "-c", """
import sys
sys.path.insert(0, sys.argv[1])
import on1x_sdk
with on1x_sdk.Manifest('on1xfile 1\\nkind library\\nname "moved"\\n') as manifest:
    assert manifest.name == "moved"
""", binding)

    project = work / "project"
    src = project / "src"
    src.mkdir()
    manifest = project / "On1xfile"
    manifest.write_text(
        'on1xfile 1\nkind executable\nname "moved"\ndirectory "src"\n'
        'target "moved" "src/main.on1x"\nprofile "tuned" "profile.lua"\n',
        encoding="utf-8")
    (src / "On1xfile.m").write_text('on1xfile 1\nsource "main.on1x"\n', encoding="utf-8")
    (src / "main.on1x").write_text(
        "fn main() -> i64 { println(42); return 0; }\n", encoding="utf-8")
    (project / "profile.lua").write_text(
        "return on1xfile.profile { optimize = 1, debug = true }\n", encoding="utf-8")

    # A caller-owned module cannot substitute for the moved SDK helper.
    (project / "on1x_optimization.py").write_text(
        'raise RuntimeError("shadow module was loaded")\n', encoding="utf-8")
    shadow = dict(env, PYTHONPATH=str(project))
    result = json.loads(run("sdk-manifest", sdk, "validate", "on1xfile", manifest,
                            cwd=project, environment=shadow))
    verify(result.get("valid") and result.get("input_schema") == "on1xfile/1",
           "SDK manifest validation did not use the installed helper")
    lock = project / "On1xfile.lock"
    lock.write_text(json.dumps({
        "schema": "on1x.lock/v4",
        "dependencies": {},
        "kernel_paths": [{"path": "vendor/kernels", "sha256": "0" * 64}],
    }), encoding="utf-8")
    result = json.loads(run("sdk-lock", sdk, "validate", "lock", lock,
                            cwd=project, environment=shadow))
    verify(result.get("valid") and result.get("input_schema") == "on1x.lock/v4",
           "SDK lock validation did not report v4")
    lock.unlink()

    helper = installed / "tools/on1x-project.py"
    disabled = helper.with_suffix(".py.disabled")
    helper.rename(disabled)
    try:
        result = json.loads(run("sdk-missing-helper", sdk, "validate", "on1xfile", manifest,
                                cwd=project, environment=shadow, returncode=3))
        verify(result.get("status") == "UNSUPPORTED"
               and result.get("message") == "installed On1x project validator is unavailable",
               "missing installed helper should reject")
    finally:
        disabled.rename(helper)

    # No --mune-runner, --libstone-root, or source driver: exercise installed
    # helper discovery through the relocated compiler and run its output.
    output = run("project-build", compiler, "aux", "build", project,
                 "--cc", args.cc, "--profile", "tuned")
    output_dir = Path(output.strip()).resolve(strict=True)
    verify(output_dir.is_relative_to(project), "project output escaped the fixture")
    report = json.loads((output_dir / ".on1x-build.json").read_text(encoding="utf-8"))
    profile = installed / "stdlib/on1xfile/profiles.lua"
    verify(report.get("cflags") == ["-O1", "-g"]
           and report["inputs"].get("stdlib/profiles.lua")
           == hashlib.sha256(profile.read_bytes()).hexdigest(),
           "build did not use the requested installed Mune policy")
    verify(run("project-run", output_dir / "moved") == "42\n",
           "relocated project produced unexpected output")

    qmr = moved / "libexec/on1x/on1x-qmr-compile"
    verify(not args.require_qmr or qmr.is_file(), "expected installed QMR helper")
    if qmr.is_file():
        generated = run("qmr-compile", qmr, "fixture.On1xfile.i",
                        input_text="role = {{ data.role }}\n")
        policy = work / "qmr-policy.lua"
        policy.write_text(generated, encoding="utf-8")
        rendered = run("qmr-render", moved / "libexec/on1x/on1x-mune-runner",
                       "render", installed / "stdlib/on1xfile/render.lua",
                       policy, "role", "moved")
        verify(rendered == 'role = "moved"\n', "unexpected installed QMR result")
    else:
        print("QMR: not installed (not required)", flush=True)
    print(f"Installed SDK and project relocation qualified: {moved}", flush=True)


if __name__ == "__main__":
    main()
