#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
tmp=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then
    tmp=/tmp/opencode
fi
B=$(mktemp -d "$tmp/mulib-sandbox-tests.XXXXXXXX")
trap 'rm -rf "$B"' EXIT HUP INT TERM
# strict production workers must not use ASan runtime (VM mapping / syscall needs).
FLAGS='-D_GNU_SOURCE -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Werror -Ikernels'
$CC $FLAGS -c tests/sandbox-implementation.c -o "$B/implementation.o"
$CC $FLAGS tests/worker.c "$B/implementation.o" -o "$B/worker"
$CC $FLAGS -DFAKE_READY tests/worker.c "$B/implementation.o" -o "$B/fake"
$CC $FLAGS tests/test_sandbox.c "$B/implementation.o" -o "$B/test"
chmod 755 "$B" # allow dropped worker identity to execute trusted image
"$B/test" "$B/worker" "$B/fake"
if [ "${SANITIZE:-0}" = 1 ]; then
    $CC $FLAGS -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
        tests/sandbox-implementation.c tests/test_sandbox.c -o "$B/test-san"
    "$B/test-san" "$B/worker" "$B/fake"
fi
