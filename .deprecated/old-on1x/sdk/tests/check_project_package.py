"""Install, relocate and consume the project SDK, CLI and Python binding."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*arguments, env=None, cwd=None):
    return subprocess.run(
        arguments, check=True, timeout=60, env=env, cwd=cwd,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main():
    (cmake, build, config, cxx, cxxflags, ldflags, libdir, bindir,
     python) = sys.argv[1:]
    build = Path(build).resolve(strict=True)
    build_config = ("--config", config) if config else ()
    with tempfile.TemporaryDirectory(
            prefix="sdk-project-package-", dir=build) as directory:
        work = Path(directory)
        original = work / "original"
        relocated = work / "relocated"
        run(cmake, "--install", str(build), *build_config,
            "--prefix", str(original), "--component", "On1xSDK")
        original.rename(relocated)

        consumer = work / "consumer"
        consumer.mkdir()
        (consumer / "main.cpp").write_text(
            r'''
#include <On1xSDK.hpp>
#include <string>
#include <vector>

int main() {
    const std::string source =
        "on1xfile 1\nkind executable\nname \"installed\"\n"
        "target \"installed\" \"main.on1x\"\n";
    auto manifest = on1x::sdk::Manifest::parse(source);
    if (manifest.name() != "installed" || manifest.target_count() != 1)
        return 1;
    if (on1x::sdk::format_manifest(source).view() != source)
        return 2;
    std::vector<on1x::sdk::BuildNode> nodes{
        {"compile", "compile", std::string(64, 'a'), {}},
        {"link", "link", std::string(64, 'b'), {"compile"}},
    };
    auto graph = on1x::sdk::BuildGraph::create(nodes);
    return graph.size() == 2 && graph.sha256().size() == 64 ? 0 : 3;
}
'''.lstrip(),
            encoding="utf-8",
        )
        (consumer / "CMakeLists.txt").write_text(
            """
cmake_minimum_required(VERSION 3.20)
project(ProjectSDKConsumer LANGUAGES CXX)
enable_testing()
find_package(On1xSDK 2 CONFIG REQUIRED)
if(NOT On1xSDK_PROJECT_ABI_VERSION EQUAL 2)
  message(FATAL_ERROR "project ABI metadata is missing")
endif()
add_executable(consumer main.cpp)
set_target_properties(consumer PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_link_libraries(consumer PRIVATE On1x::Project)
add_test(NAME consumer COMMAND consumer)
""".lstrip(),
            encoding="utf-8",
        )
        output = work / "consumer-build"
        run(cmake, "-S", str(consumer), "-B", str(output),
            f"-DCMAKE_PREFIX_PATH={relocated}",
            f"-DCMAKE_BUILD_TYPE={config}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DCMAKE_CXX_FLAGS={cxxflags}",
            f"-DCMAKE_EXE_LINKER_FLAGS={ldflags}")
        run(cmake, "--build", str(output), *build_config)
        ctest = str(Path(cmake).with_name("ctest"))
        run(ctest, "--test-dir", str(output), "--output-on-failure",
            *(("-C", config) if config else ()))

        tool = relocated / bindir / "on1x-sdk"
        info = json.loads(run(str(tool), "info").stdout)
        if (info.get("schema") != "on1x.sdk.info/1"
                or info.get("project_abi") != 2):
            raise RuntimeError("installed on1x-sdk reported unexpected metadata")
        manifest = work / "On1xfile"
        manifest.write_text(
            "on1xfile 1\nkind library\nname \"relocated\"\n"
            "optimization-profile debug\noptimization \"dev\" \"all\" off\n",
            encoding="utf-8",
        )
        # Neither the current project nor PYTHONPATH supplies trusted SDK helpers.
        (work / "on1x_optimization.py").write_text("raise RuntimeError('shadowed SDK helper')\n")
        validated = json.loads(run(str(tool), "validate", "on1xfile", str(manifest),
                                   cwd=work, env=dict(os.environ, PYTHONPATH=str(work))).stdout)
        if not validated.get("valid"):
            raise RuntimeError("installed project validator rejected a manifest")
        lock = work / "On1xfile.lock"
        lock.write_text(json.dumps({
            "schema": "on1x.lock/v4",
            "dependencies": {},
            "kernel_paths": [{"path": "vendor/kernels", "sha256": "0" * 64}],
        }), encoding="utf-8")
        locked = json.loads(run(str(tool), "validate", "lock", str(lock),
                               cwd=work, env=dict(os.environ, PYTHONPATH=str(work))).stdout)
        if not locked.get("valid") or locked.get("input_schema") != "on1x.lock/v4":
            raise RuntimeError("installed project validator did not report the exact lock schema")

        binding = relocated / libdir / "on1x-sdk/python"
        snippet = (
            "import sys;"
            f"sys.path.insert(0,{str(binding)!r});"
            "import on1x_sdk;"
            "m=on1x_sdk.Manifest("
            "'on1xfile 1\\nkind library\\nname \"relocated\"\\n');"
            "assert m.name=='relocated';m.close()"
        )
        environment = {
            "PATH": os.defpath,
            "LC_ALL": "C",
            "PYTHONDONTWRITEBYTECODE": "1",
        }
        run(python, "-I", "-c", snippet, env=environment, cwd=work)


if __name__ == "__main__":
    main()
