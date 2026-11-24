#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
: "${CC:=cc}"
: "${CFLAGS:=-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror}"
: "${CXXFLAGS:=-std=c++17 -Wall -Wextra -Wpedantic -Werror}"
: "${LDFLAGS:=}"
work=$(mktemp -d)
trap 'rm -f "$work/test" "$work/impl.o" "$work"/module.c "$work"/mod-*.o; rmdir "$work"' EXIT HUP INT TERM
for suite in markup-ir markup-tree-stream markup-spans-graph-bridge markup-alloc; do
  "$CC" $CFLAGS -Ikernels "tests/test-$suite.c" $LDFLAGS -o "$work/test"
  "$work/test"
done
"$CC" $CFLAGS -Ikernels -c tests/fixtures/markup-ir-implementation.c -o "$work/impl.o"
# Link C11 implementation with a distinct C++17 declaration-only consumer.
if command -v "${CXX:-c++}" >/dev/null 2>&1; then
  "${CXX:-c++}" $CXXFLAGS -Ikernels tests/markup-ir-consumer.cpp "$work/impl.o" $LDFLAGS -o "$work/test"
  "$work/test"
fi
# Each module must also compile independently of the other implementations.
for mod in tree stream spans graph bridge; do
  macro=$(printf '%s' "$mod" | tr '[:lower:]' '[:upper:]')
  printf '#include "mulib-markup-%s.h"\n#define MULIB_IMPL_CDD_MARKUP_%s\n#include "mulib-markup-%s.h"\n' \
    "$mod" "$macro" "$mod" > "$work/module.c"
  "$CC" $CFLAGS -Ikernels -c "$work/module.c" -o "$work/mod-$mod.o"
done
if command -v "${CXX:-c++}" >/dev/null 2>&1; then
  "${CXX:-c++}" $CXXFLAGS -Ikernels tests/markup-ir-consumer.cpp "$work"/mod-*.o $LDFLAGS -o "$work/test"
  "$work/test"
fi
