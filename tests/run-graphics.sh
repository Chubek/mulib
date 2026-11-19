#!/bin/sh
# Device-free regression gate for the additive graphics overlay.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
CC=${CC:-cc}
CXX=${CXX:-c++}
PYTHON=${PYTHON:-python3}
TEMP_PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then TEMP_PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$TEMP_PARENT/mulib-graphics.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT HUP INT TERM
WARN="-Wall -Wextra -Wpedantic -Werror -Wno-misleading-indentation -UNDEBUG"
FLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L $WARN ${CFLAGS:-}"
LINK="${LDFLAGS:-} -lm -ldl -pthread"
CXX_SANITIZERS=""
if [ "${SANITIZE:-0}" = 1 ]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
    LINK="$LINK -fsanitize=address,undefined"
    CXX_SANITIZERS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
fi
"$PYTHON" tools/graphics_inventory.py --check
"$PYTHON" tools/graphics_inventory.py --list-headers > "$BUILD/headers"
: > "$BUILD/combined.c"
headers=0
while IFS= read -r header; do
    printf '#include "%s"\n#include "%s"\n' "$header" "$header" > "$BUILD/header.c"
    # Deliberate word splitting permits compiler flags, not source paths.
    # shellcheck disable=SC2086
    "$CC" $FLAGS -I"$ROOT/include" -fsyntax-only "$BUILD/header.c"
    printf '#include "%s"\n' "$header" >> "$BUILD/combined.c"
    headers=$((headers + 1))
done < "$BUILD/headers"
printf 'int main(void) { return 0; }\n' >> "$BUILD/combined.c"
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/include" "$BUILD/combined.c" $LINK -o "$BUILD/combined"
"$BUILD/combined"
# Optional adapters must also parse against the actual upstream declarations.
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/include" -fsyntax-only "$ROOT/tests/graphics/bridges.c"
printf 'PASS: %s standalone headers and combined C11 linkage\n' "$headers"
tests=0
for source in "$ROOT"/tests/test-gfx-*.c \
    "$ROOT/tests/gfx-immgtk-alloc.c" "$ROOT/tests/gfx-immgtk-link.c" \
    "$ROOT/tests/gfx-immgtk-mune.c" "$ROOT/tests/gfx-drm-prime-no-cloexec.c"; do
    [ -f "$source" ] || continue
    name=$(basename "$source" .c)
    # Keep multi-TU regressions real rather than textually including peers.
    case "$name" in
        test-gfx-input)
            set -- "$ROOT/tests/gfx-input-tu.c" "$ROOT/tests/gfx-input-unix.c" \
                "$ROOT/tests/gfx-input-evdev.c" ;;
        test-gfx-retgtk-core)
            set -- "$ROOT/tests/gfx-retgtk-core-types-a.c" \
                "$ROOT/tests/gfx-retgtk-core-types-b.c" ;;
        test-gfx-retgtk-model-layout)
            set -- "$ROOT/tests/gfx-retgtk-model-layout-peer.c" ;;
        gfx-immgtk-link) set -- "$ROOT/tests/gfx-immgtk-link-peer.c" ;;
        *) set -- ;;
    esac
    # shellcheck disable=SC2086
    "$CC" $FLAGS -I"$ROOT/include" "$source" "$@" $LINK -o "$BUILD/$name"
    "$BUILD/$name"
    tests=$((tests + 1))
done
# Existing pixel/behavior contracts, including the actual Mune legacy host.
if [ "${LEGACY:-1}" = 1 ]; then
    for name in test_immgtk test_immgtk_canvas test_immgtk_drawlist test_immgtk_plot; do
        # shellcheck disable=SC2086
        "$CC" $FLAGS -I"$ROOT/include" "$ROOT/tests/$name.c" $LINK -o "$BUILD/$name"
        "$BUILD/$name" "$ROOT/library"
    done
    # Run the scripted extension regression through the actual Mune parser
    # and bytecode VM as well as the stock-Lua run below.
    # shellcheck disable=SC2086
    "$CC" $FLAGS -I"$ROOT/include" "$ROOT/tests/test_mune_runtime.c" \
        $LINK -o "$BUILD/mune-runtime"
    "$BUILD/mune-runtime" --run tests/test-gfx-immgtk-lua.lua
    "$BUILD/mune-runtime" --vm tests/test-gfx-immgtk-lua.lua
fi
# Exercise independent C TUs and a C++ consumer of an explicit C ABI boundary.
# These C11 headers are NOT thereby certified as directly C++-compilable.
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/include" -I"$ROOT/tests/graphics" \
    -c "$ROOT/tests/graphics/producer.c" -o "$BUILD/producer.o"
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/tests/graphics" "$ROOT/tests/graphics/consumer.c" \
    "$BUILD/producer.o" $LINK -o "$BUILD/link-c"
"$BUILD/link-c"
if command -v "$CXX" >/dev/null 2>&1; then
    # shellcheck disable=SC2086
    "$CXX" -std=c++17 $WARN ${CXXFLAGS:-} $CXX_SANITIZERS -I"$ROOT/tests/graphics" \
        "$ROOT/tests/graphics/consumer.cpp" "$BUILD/producer.o" $LINK -o "$BUILD/link-cxx"
    "$BUILD/link-cxx"
    if [ -f "$ROOT/tests/gfx-immgtk-legacy-cxx.cpp" ]; then
        # shellcheck disable=SC2086
        "$CXX" -std=c++17 $WARN ${CXXFLAGS:-} $CXX_SANITIZERS -I"$ROOT/include" \
            "$ROOT/tests/gfx-immgtk-legacy-cxx.cpp" $LINK -o "$BUILD/legacy-cxx"
        "$BUILD/legacy-cxx"
    fi
    # The legacy PNG header remains directly C++-includable, independently of
    # the C11-only GLX overlay. Exercise original encoder and additive decoder.
    # shellcheck disable=SC2086
    "$CC" $FLAGS -I"$ROOT/include" -c "$ROOT/tests/graphics/png-producer.c" \
        -o "$BUILD/png-producer.o"
    # shellcheck disable=SC2086
    "$CXX" -std=c++17 $WARN ${CXXFLAGS:-} $CXX_SANITIZERS -I"$ROOT/include" \
        "$ROOT/tests/graphics/png-consumer.cpp" "$BUILD/png-producer.o" \
        $LINK -o "$BUILD/png-cxx"
    "$BUILD/png-cxx"
else
    printf 'SKIP: C++ ABI consumer (compiler unavailable)\n'
fi
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/include" "$ROOT/examples/gfx-headless.c" $LINK -o "$BUILD/demo"
"$BUILD/demo" "$BUILD/demo.ppm"
LUA=${LUA:-lua}
if command -v "$LUA" >/dev/null 2>&1; then
    lua_tests=0
    for source in "$ROOT"/tests/test-gfx-*.lua; do
        [ -f "$source" ] || continue
        "$LUA" "$source" "$ROOT/library"
        lua_tests=$((lua_tests + 1))
    done
    printf 'PASS: %s stock-Lua suites (not Mune parity certification)\n' "$lua_tests"
else
    printf 'SKIP: stock-Lua suites (runtime unavailable)\n'
fi
printf 'PASS: %s graphics C suites, ABI linkage, headless demo and requested legacy regressions\n' "$tests"
