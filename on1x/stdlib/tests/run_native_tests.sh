#!/bin/sh
set -eu
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH='' cd -- "$HERE/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
: "${CC:=cc}"
: "${CFLAGS:=-std=c11 -Wall -Wextra -Werror -pedantic}"
: "${LDFLAGS:=}"
for name in test_native test_real_native test_native_failures; do
  # CFLAGS and LDFLAGS intentionally split as compiler flag words.
  "$CC" $CFLAGS "$ROOT/runtime/basis_native.c" \
    "$ROOT/runtime/basis_real.c" "$HERE/$name.c" -lm $LDFLAGS -o "$TMP/$name"
  "$TMP/$name"
done
python3 "$HERE/check_package.py"
