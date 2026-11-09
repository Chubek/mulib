#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
temp_parent=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then
  temp_parent=/tmp/opencode
fi
work=$(mktemp -d "$temp_parent/mulib-web-tests.XXXXXX")
trap 'rm -f "$work"/*; rmdir "$work"' EXIT HUP INT TERM
for f in tests/test-*.c tests/test_web_additions.c; do
  n=$(basename "$f" .c)
  ${CC:-cc} -std=c11 -Wall -Wextra -Werror -pedantic ${CFLAGS:-} -Iinclude "$f" -o "$work/$n"
  "$work/$n"
done
echo "All MuLIB web module tests passed."
