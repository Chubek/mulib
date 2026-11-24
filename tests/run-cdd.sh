#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
CXX=${CXX:-c++}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
CXXFLAGS=${CXXFLAGS-${CFLAGS:-}}
# CFLAGS, CXXFLAGS, and LDFLAGS may supply sanitizers or other compiler flags.
WARN='-Wall -Wextra -Wpedantic -Werror'
$CC -std=c11 $WARN ${CFLAGS:-} -Ikernels tests/cdd-implementation.c tests/cdd-test.c ${LDFLAGS:-} -o "$TMP/test"
"$TMP/test"
# Test allocator failure without adding a hook to the library. GNU-compatible
# linkers support --wrap; the rest of the suite does not rely on this option.
printf 'int main(void) { return 0; }\n' > "$TMP/link.c"
if $CC -std=c11 ${CFLAGS:-} "$TMP/link.c" ${LDFLAGS:-} -Wl,--wrap=calloc -o "$TMP/link" 2>/dev/null; then
    $CC -std=c11 $WARN ${CFLAGS:-} -Ikernels tests/cdd-implementation.c tests/cdd-nomem.c ${LDFLAGS:-} -Wl,--wrap=calloc -o "$TMP/nomem"
    "$TMP/nomem"
else
    echo 'CDD allocation failure test skipped (linker does not support --wrap=calloc)'
fi
$CC -std=c11 $WARN ${CFLAGS:-} -Ikernels examples/cdd-pipeline.c ${LDFLAGS:-} -o "$TMP/example"
"$TMP/example"
# Verify standalone declarations and delayed implementation switches per header.
for h in kernels/mulib-cdd-*.h; do
    basename=${h##*/}
    macro=$(printf '%s' "${basename%.h}" | tr '[:lower:]-' '[:upper:]_')
    macro=MULIB_IMPL_${macro#MULIB_}
    printf '#include "%s"\n#define %s\n#include "%s"\n#include "%s"\n' "$basename" "$macro" "$basename" "$basename" > "$TMP/header.c"
    $CC -std=c11 $WARN ${CFLAGS:-} -Ikernels -c "$TMP/header.c" -o "$TMP/header.o"
    $CXX -x c++ -std=c++17 $WARN $CXXFLAGS -Ikernels -c "$TMP/header.c" -o "$TMP/header-cpp.o"
done
# Strict C++17 compile of the implementation; run the same tests through C linkage.
$CC -std=c11 $WARN ${CFLAGS:-} -Ikernels -c tests/cdd-test.c -o "$TMP/test.o"
$CXX -x c++ -std=c++17 $WARN $CXXFLAGS -Ikernels -c tests/cdd-implementation.c -o "$TMP/impl.o"
$CXX $CXXFLAGS "$TMP/test.o" "$TMP/impl.o" ${LDFLAGS:-} -o "$TMP/cpp-test"
"$TMP/cpp-test"
