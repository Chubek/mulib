#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
temp_parent=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then temp_parent=/tmp/opencode; fi
work=$(mktemp -d "$temp_parent/mulib-api-kernels.XXXXXX")
trap 'rm -f "$work"/module.c "$work"/*.o "$work"/check; rmdir "$work"' EXIT HUP INT TERM
cc=${CC:-cc}
cxx=${CXX:-c++}
warn='-Wall -Wextra -Wpedantic -Werror -UNDEBUG'
# These two fixtures instantiate their own implementation switches.
for mod in grpc soap; do
  source="tests/test-api-$mod.c"
  "$cc" -std=c11 $warn ${CFLAGS:-} -Iinclude "$source" ${LDFLAGS:-} \
    -lm -ldl -pthread -o "$work/check"
  "$work/check"
done
"$cc" -std=c11 $warn ${CFLAGS:-} -Iinclude -c tests/api-kernels/dependencies.c -o "$work/deps.o"
# Delayed definitions and independent C TUs exercise both switch spellings,
# including modules whose declarations include another of the seven headers.
for flavor in canonical legacy; do
  for mod in graphql grpc jsonrpc rest soap sse-client websocket; do
    macro=$(printf '%s' "$mod" | tr '[:lower:]-' '[:upper:]_')
    prefix=MULIB_IMPL_
    if [ "$flavor" = canonical ]; then prefix=MULIB_IMPL_CDD_; fi
    printf '#include "mulib-%s.h"\n#define %s%s\n#include "mulib-%s.h"\n#include "mulib-%s.h"\n' \
      "$mod" "$prefix" "$macro" "$mod" "$mod" > "$work/module.c"
    "$cc" -std=c11 $warn ${CFLAGS:-} -Iinclude -c "$work/module.c" -o "$work/$mod.o"
  done
  "$cxx" -std=c++17 $warn ${CXXFLAGS:-} -Iinclude tests/api-kernels/link.cpp \
    "$work"/*.o ${LDFLAGS:-} -lm -ldl -pthread -o "$work/check"
  "$work/check"
  if [ "$flavor" = canonical ]; then
    for mod in json-http sse-client websocket; do
      "$cc" -std=c11 $warn ${CFLAGS:-} -Iinclude "tests/test-api-$mod.c" \
        "$work"/*.o ${LDFLAGS:-} -lm -ldl -pthread -o "$work/check"
      "$work/check"
    done
  fi
done
printf '%s\n' 'Seven protocol kernels: canonical and legacy switches, independent C TUs and C++17 linkage passed.'
