"""Exercise the descriptor SDK as relocated C11 and C++17 clients."""

import pathlib
import shutil
import subprocess
import sys
import tempfile


def run(*args):
    subprocess.run(args, check=True, timeout=90)


def main():
    cmake, build, config, cc, cxx, cflags, cxxflags, ldflags = sys.argv[1:]
    build_config = ["--config", config] if config else []
    tests = pathlib.Path(__file__).parent
    with tempfile.TemporaryDirectory(prefix="sdk-ffi-install-", dir=build) as directory:
        work = pathlib.Path(directory)
        initial = work / "install"
        prefix = work / "relocated"
        run(cmake, "--install", build, *build_config,
            "--prefix", str(initial), "--component", "On1xSDK")
        initial.rename(prefix)
        source = work / "client"
        source.mkdir()
        for lang in ("c", "cpp"):
            shutil.copyfile(tests / f"ffi_descriptor.{lang}", source / f"lookup.{lang}")
        (source / "CMakeLists.txt").write_text(
            """cmake_minimum_required(VERSION 3.20)
project(DescriptorClients LANGUAGES C CXX)
enable_testing()
find_package(On1xSDK 2 CONFIG REQUIRED PATHS "${CMAKE_PREFIX_PATH}" NO_DEFAULT_PATH)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)
foreach(lang IN ITEMS c cpp)
  add_executable(lookup_${lang} lookup.${lang})
  target_link_libraries(lookup_${lang} PRIVATE On1x::SDK)
  add_test(NAME lookup_${lang} COMMAND lookup_${lang})
endforeach()
""",
            encoding="utf-8",
        )
        output = work / "build"
        run(cmake, "-S", str(source), "-B", str(output),
            f"-DCMAKE_PREFIX_PATH={prefix}", f"-DCMAKE_BUILD_TYPE={config}",
            f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DCMAKE_C_FLAGS={cflags}", f"-DCMAKE_CXX_FLAGS={cxxflags}",
            f"-DCMAKE_EXE_LINKER_FLAGS={ldflags}")
        run(cmake, "--build", str(output), *build_config)
        run(str(pathlib.Path(cmake).with_name("ctest")),
            "--test-dir", str(output), "--output-on-failure", "--no-tests=error",
            *(["-C", config] if config else []))


if __name__ == "__main__":
    main()
