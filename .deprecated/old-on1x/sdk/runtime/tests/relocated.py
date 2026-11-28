"""Exercise the relocated 1.1 package with a C++ host and Python auto-discovery."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


cmake, build, config, cc, cxx, generated = sys.argv[1:]
tests = Path(__file__).resolve().parent
config_args = ["--config", config] if config else []


def run(*args, **options):
    subprocess.run(args, check=True, timeout=60, **options)


with tempfile.TemporaryDirectory(dir=build, prefix="relocated-rt-") as tmp:
    work = Path(tmp)
    prefix = work / "moved"
    consumer = work / "consumer"
    run(cmake, "--install", build, *config_args, "--prefix", work / "original",
        "--component", "On1xRT")
    (work / "original").rename(prefix)
    (work / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.20)
project(RelocatedHost LANGUAGES C CXX)
find_package(On1xRT 1.1 CONFIG REQUIRED)
add_library(provider SHARED "{tests}/choose_provider.c")
target_include_directories(provider PRIVATE "{tests.parents[1]}/runtime" "{Path(generated).parent}")
target_link_libraries(provider PUBLIC On1x::RT)
add_executable(host "{tests}/embedding.cpp")
target_compile_features(host PRIVATE cxx_std_17)
target_link_libraries(host PRIVATE provider)
""", encoding="utf-8")
    run(cmake, "-S", work, "-B", consumer, f"-DCMAKE_PREFIX_PATH={prefix}",
        f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}")
    run(cmake, "--build", consumer)
    run(consumer / "host")
    env = {**os.environ, "PYTHONPATH": str(next(prefix.rglob("on1x_rt.py")).parent)}
    env.pop("ON1X_RT_LIBRARY", None)
    run(sys.executable, "-B", "-c", """
from on1x_rt import Host
import ctypes, sys
with Host() as h:
    h.register_static(ctypes.CDLL(sys.argv[1]))
    h.instantiate()
    if h.call_i64('choose', 21, 42) != 42:
        raise RuntimeError('wrong result')
""", consumer / "libprovider.so", env=env)
