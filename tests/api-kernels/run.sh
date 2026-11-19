#!/bin/sh
set -eu
cc=${CC:-cc}
if [ "$#" -ne 1 ] || [ ! -f "$1/mulib-graphql.h" ]; then
  printf 'usage: %s /path/to/original/protcol-kern/include\n' "$0" >&2
  exit 2
fi
original=$(CDPATH= cd -- "$1" && pwd)
cd "$(dirname "$0")/../.."
temp_parent=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then temp_parent=/tmp/opencode; fi
tmpdir=$(mktemp -d "$temp_parent/mulib-api-original.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
"$cc" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
  -Itests/api-kernels/stubs -I"$original" tests/api-kernels/syntax.c
"$cc" -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Itests/api-kernels/stubs -I"$original" tests/api-kernels/wire.c -o "$tmpdir/wire"
"$tmpdir/wire"
printf 'PASS: seven header implementation sections compile in strict C11 mode\n'
