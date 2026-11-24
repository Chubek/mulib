#!/bin/sh
# Device-free notebook integration gate. Compile example and test as separate TUs.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
CC=${CC:-cc}
TEMP_PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then TEMP_PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$TEMP_PARENT/mulib-notebook-headless.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT HUP INT TERM
FLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror -UNDEBUG ${CFLAGS:-}"
LINK="${LDFLAGS:-} -lm -ldl"
if [ "${SANITIZE:-0}" = 1 ]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
    LINK="$LINK -fsanitize=address,undefined"
fi
# Deliberate word splitting permits compiler flags, not source paths.
# shellcheck disable=SC2086
"$CC" $FLAGS -DMULIB_NOTEBOOK_HEADLESS_NO_MAIN -Ikernels -Iexamples \
    examples/notebook-headless.c tests/test-notebook-headless.c $LINK -o "$BUILD/test"
"$BUILD/test" "$BUILD/document.mnb" "$BUILD/must-not-exist"
# shellcheck disable=SC2086
"$CC" $FLAGS -Ikernels examples/notebook-headless.c $LINK -o "$BUILD/demo"
"$BUILD/demo" --ppm "$BUILD/top.ppm" --save "$BUILD/fixture.mnb"
"$BUILD/demo" --scroll 220 --ppm "$BUILD/scrolled.ppm"
"$BUILD/demo" --load "$BUILD/fixture.mnb" --save "$BUILD/reopened.mnb"
cmp "$BUILD/fixture.mnb" "$BUILD/reopened.mnb"

# Portable CRC and byte count of the PPM file (builtin font, logical RGB bytes).
check_ppm() {
    path=$1
    expected=$2
    set -- $(cksum < "$path")
    if [ "$1 $2" != "$expected" ]; then
        printf '%s: expected CRC/size %s, got %s %s\n' "$path" "$expected" "$1" "$2" >&2
        exit 1
    fi
}
check_ppm "$BUILD/top.ppm" "2210937131 460815"
check_ppm "$BUILD/scrolled.ppm" "1122686400 460815"
printf 'PASS: notebook M0 CLI, portable screenshot fixtures and bytewise roundtrip\n'
