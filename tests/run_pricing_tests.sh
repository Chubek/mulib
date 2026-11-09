#!/bin/sh
# Run from any directory: sh tests/run_pricing_tests.sh
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-cc}
CXX=${CXX:-c++}
TMPDIR=${TMPDIR:-/tmp/opencode}
if [ ! -d "$TMPDIR" ]; then TMPDIR=/tmp; fi
DIR=$(mktemp -d "$TMPDIR/mulib-pricing.XXXXXX")
trap 'rm -rf "$DIR"' 0
trap 'exit 1' HUP INT TERM
# CFLAGS/CXXFLAGS/LDFLAGS are intentionally word-split like compiler flags in make.
# Set SANITIZERS=1 to check all C tests and the mixed-language ABI under ASan/UBSan.
SAN_FLAGS=
if [ "${SANITIZERS:-0}" = 1 ]; then
  SAN_FLAGS='-g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer'
fi
for test in test_price_statistics test_competprice test_histprice test_priceopt test_sat test_smt test_sat_oracle test_smt_oracle test_pricing_properties test_pricing_failures; do
  "$CC" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror ${CFLAGS:-} $SAN_FLAGS -UNDEBUG -Iinclude "tests/${test}.c" ${LDFLAGS:-} -lm -o "$DIR/$test"
  "$DIR/$test"
done
"$CC" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror ${CFLAGS:-} $SAN_FLAGS -Iinclude -c tests/price_implementations.c -o "$DIR/implementations.o"
"$CXX" -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror ${CXXFLAGS:-} $SAN_FLAGS -UNDEBUG -Iinclude tests/test_pricing_cpp.cpp "$DIR/implementations.o" ${LDFLAGS:-} -lm -o "$DIR/cpp"
"$DIR/cpp"
printf '%s\n' 'pricing tests including C++ ABI passed'
