#!/bin/sh
# Focused EDA suite; the complete MuLIB suite is run by tools/check.py or CTest.
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CC="${CC:-cc}"
CXX="${CXX:-c++}"
: "${CFLAGS:=-std=c11 -O1 -Wall -Wextra -Wpedantic -Werror}"
: "${CXXFLAGS:=-std=c++17 -O1 -Wall -Wextra -Wpedantic -Werror}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
n=0
# The aggregate implementation defines the scope, independently of the manifest.
sed -n 's/^#include "mulib-\(.*\)\.h"$/\1/p' tests/amb3r_implementation.c |
  tr '-' '_' > "$WORK/modules"
printf '%s\n' all_headers amb3r_vertical eda_extended eda_alloc >> "$WORK/modules"
while IFS= read -r module; do
  src="tests/test_${module}.c"
  "$CC" $CFLAGS -Iinclude "$src" -lm -o "$WORK/prog"
  "$WORK/prog" >/dev/null
  n=$((n+1))
done < "$WORK/modules"
"$CXX" $CXXFLAGS -Iinclude tests/test_all_headers_cpp.cpp -o "$WORK/prog_cpp"
"$WORK/prog_cpp"
"$CC" $CFLAGS -Iinclude -c tests/amb3r_implementation.c -o "$WORK/implementation.o"
"$CXX" $CXXFLAGS -Iinclude tests/test_cpp_link.cpp "$WORK/implementation.o" -lm -o "$WORK/link_cpp"
"$WORK/link_cpp"
printf 'PASS: %s C regression/integration tests, C++ declarations and C/C++ linkage\n' "$n"
