"""Exercise the installed SDK after relocating it; no source-tree headers."""

import pathlib
import shutil
import subprocess
import sys
import tempfile


def run(*args):
    subprocess.run(args, check=True, timeout=60)


def main():
    cmake, build, config, cc, cxx, cflags, cxxflags, ldflags = sys.argv[1:]
    build_config = ["--config", config] if config else []
    with tempfile.TemporaryDirectory(prefix="sdk-package-", dir=build) as directory:
        work = pathlib.Path(directory)
        install = work / "original"
        relocated = work / "moved"
        run(cmake, "--install", build, *build_config,
            "--prefix", str(install), "--component", "On1xSDK")
        install.rename(relocated)
        consumer = work / "consumer"
        consumer.mkdir()
        shutil.copyfile(pathlib.Path(__file__).with_name("c_abi.c"), consumer / "main.c")
        (consumer / "CMakeLists.txt").write_text(
            """cmake_minimum_required(VERSION 3.20)
project(SDKConsumer LANGUAGES C CXX)
enable_testing()
find_package(On1xSDK 1 CONFIG REQUIRED)
add_executable(consumer main.c)
set_target_properties(consumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(consumer PRIVATE On1x::SDK)
add_test(NAME consumer COMMAND consumer)
""", encoding="utf-8")
        output = work / "consumer-build"
        run(cmake, "-S", str(consumer), "-B", str(output),
            f"-DCMAKE_PREFIX_PATH={relocated}", f"-DCMAKE_BUILD_TYPE={config}",
            f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
            f"-DCMAKE_C_FLAGS={cflags}", f"-DCMAKE_CXX_FLAGS={cxxflags}",
            f"-DCMAKE_EXE_LINKER_FLAGS={ldflags}")
        run(cmake, "--build", str(output), *build_config)
        ctest = str(pathlib.Path(cmake).with_name("ctest"))
        run(ctest, "--test-dir", str(output), "--output-on-failure",
            *(["-C", config] if config else []))


if __name__ == "__main__":
    main()
