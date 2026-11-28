"""Build and run a relocated installed consumer of the frontend C API."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(*args):
    subprocess.run(args, check=True, timeout=120)


def main():
    cmake, build, config, cc, cxx, source, *module_test = sys.argv[1:]
    build_config = ["--config", config] if config else []
    with tempfile.TemporaryDirectory(prefix="frontend-package-", dir=build) as directory:
        work = Path(directory)
        original = work / "original"
        relocated = work / "relocated"
        run(
            cmake,
            "--install",
            build,
            *build_config,
            "--prefix",
            str(original),
            "--component",
            "On1xSDK",
        )
        original.rename(relocated)
        consumer = work / "consumer"
        consumer.mkdir()
        shutil.copyfile(source, consumer / "main.c")
        consumer_cmake = """cmake_minimum_required(VERSION 3.20)
project(FrontendConsumer LANGUAGES C CXX)
enable_testing()
find_package(On1xSDK 2 CONFIG REQUIRED)
add_executable(frontend main.c)
set_target_properties(frontend PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(frontend PRIVATE On1x::SDK)
add_test(NAME frontend COMMAND frontend)
"""
        if module_test:
            [module_source, module_runner] = module_test
            shutil.copyfile(module_source, consumer / "module_logistics.c")
            shutil.copyfile(module_runner, consumer / "module_logistics.py")
            consumer_cmake += f"""
add_executable(module_logistics module_logistics.c)
set_target_properties(module_logistics PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_link_libraries(module_logistics PRIVATE On1x::SDK)
add_test(NAME module_logistics COMMAND "{Path(sys.executable).as_posix()}"
  "${{CMAKE_CURRENT_SOURCE_DIR}}/module_logistics.py" "$<TARGET_FILE:module_logistics>")
"""
        (consumer / "CMakeLists.txt").write_text(consumer_cmake, encoding="utf-8")
        output = work / "build"
        run(
            cmake,
            "-S",
            str(consumer),
            "-B",
            str(output),
            f"-DCMAKE_PREFIX_PATH={relocated}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
        )
        run(cmake, "--build", str(output), *build_config)
        run(str(Path(cmake).with_name("ctest")), "--test-dir", str(output),
            "--output-on-failure", *(["-C", config] if config else []))


if __name__ == "__main__":
    main()
