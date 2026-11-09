#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Use the canonical build: some existing suites need libraries, fixtures or
# target-specific definitions and cannot be compiled with a generic cc loop.
build=${BUILD_DIR:-tests/build}
cmake -S . -B "$build" -DMULIB_BUILD_SHIRAZ=OFF \
    -DMULIB_SANITIZERS="${SANITIZERS:-OFF}" \
    -DCMAKE_C_FLAGS="${CFLAGS:-}"
cmake --build "$build" --parallel "${JOBS:-4}"
ctest --test-dir "$build" --output-on-failure --parallel "${JOBS:-4}"
