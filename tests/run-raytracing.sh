#!/bin/sh
# Dedicated device-free raytracing suites, including the legacy API.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TEMP_PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then TEMP_PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$TEMP_PARENT/mulib-raytracing.XXXXXX")
trap 'rm -f "$BUILD/path" "$BUILD/legacy"; rmdir "$BUILD"' EXIT
trap 'exit 1' HUP INT TERM
CC=${CC:-cc}
FLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror ${CFLAGS:-} -UNDEBUG"
if [ "${SANITIZE:-0}" = 1 ]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
fi
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/include" "$ROOT/tests/test_raytracing.c" \
    "$ROOT/tests/raytracing-implementation.c" ${LDFLAGS:-} -lm -o "$BUILD/path"
"$BUILD/path"
# shellcheck disable=SC2086
"$CC" $FLAGS -I"$ROOT/include" "$ROOT/tests/test_graphics_render.c" \
    ${LDFLAGS:-} -lm -o "$BUILD/legacy"
"$BUILD/legacy"
printf 'PASS: seeded path tracing and legacy raytracing regressions\n'
