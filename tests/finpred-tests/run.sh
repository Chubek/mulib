#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
CXX="${CXX:-c++}"
if [ -n "${MULIB_ROOT:-}" ] || [ -f kernels/mulib-money.h ]; then
    MULIB_ROOT="${MULIB_ROOT:-$PWD}"
    MONEY_HEADER="$MULIB_ROOT/kernels/mulib-money.h"
    test -f "$MONEY_HEADER" || { echo 'MULIB_ROOT/kernels/mulib-money.h missing' >&2; exit 1; }
    DEP_INC="-I$MULIB_ROOT/kernels"
    echo 'Testing against canonical MuLIB Money:' "$MONEY_HEADER"
    set -- -DMULIB_IMPL_MONEY
else
    DEP_INC="-isystem$PWD/tests/finpred-tests/support"
    MONEY_HEADER=mulib-money.h
    echo 'TEST-ONLY fallback Money fixture. Canonical SHA must still be available in kernels/.'
    set --
fi
DIR_TEST=tests/finpred-tests
DIR="$(mktemp -d "${TMPDIR:-/tmp}/finpred-test.XXXXXX")"
trap 'rm -rf "$DIR"' EXIT INT TERM
CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror'
CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror'
if [ "${SANITIZE:-0}" = 1 ]; then
  SAN_FLAGS='-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer'
  CFLAGS="$CFLAGS $SAN_FLAGS"
  CXXFLAGS="$CXXFLAGS $SAN_FLAGS"
  LDFLAGS="$SAN_FLAGS"
else
  LDFLAGS=''
fi
# One aggregate C11 implementation; all ordinary C/C++ clients use declarations.
# shellcheck disable=SC2086
"$CC" $CFLAGS "$@" -Ikernels "$DEP_INC" -include "$MONEY_HEADER" -c "$DIR_TEST/test_kernels_impl.c" -o "$DIR/impl.o"
# shellcheck disable=SC2086
"$CC" $CFLAGS -Ikernels "$DEP_INC" -include "$MONEY_HEADER" "$DIR_TEST/test_all.c" "$DIR/impl.o" -lm $LDFLAGS -o "$DIR/test_all"
"$DIR/test_all"
# The exhaustive StayMatch oracle instantiates its own independent test binary.
# shellcheck disable=SC2086
"$CC" $CFLAGS "$@" -Ikernels "$DEP_INC" -include "$MONEY_HEADER" "$DIR_TEST/test_staymatch_oracle.c" -lm $LDFLAGS -o "$DIR/oracle"
"$DIR/oracle"
# shellcheck disable=SC2086
"$CC" $CFLAGS -Ikernels "$DEP_INC" -include "$MONEY_HEADER" "$DIR_TEST/test_fp_boundaries.c" "$DIR/impl.o" \
    -Wl,--wrap=malloc -lm $LDFLAGS -o "$DIR/boundaries"
"$DIR/boundaries"
if [ "$#" -ne 0 ]; then
  # P0 also exercises canonical StatsProc and its quantile semantics.
  # shellcheck disable=SC2086
  "$CC" $CFLAGS "$@" -Ikernels "$DEP_INC" -include "$MONEY_HEADER" \
    "$DIR_TEST/test_p0.c" -lm $LDFLAGS -o "$DIR/test_p0"
  "$DIR/test_p0"
  # shellcheck disable=SC2086
  "$CC" $CFLAGS -Ikernels "$DEP_INC" -include "$MONEY_HEADER" \
    "$DIR_TEST/test_fp_money_client.c" "$DIR/impl.o" -lm $LDFLAGS -o "$DIR/money_client"
  python3 "$DIR_TEST/test_fp_money_oracle.py" "$DIR/money_client"
fi
# shellcheck disable=SC2086
"$CXX" $CXXFLAGS -Ikernels "$DEP_INC" -include "$MONEY_HEADER" \
    "$DIR_TEST/test_link_cpp.cpp" "$DIR/impl.o" -lm $LDFLAGS -o "$DIR/link_cpp"
"$DIR/link_cpp"
# shellcheck disable=SC2086
"$CXX" $CXXFLAGS -Ikernels "$DEP_INC" -include "$MONEY_HEADER" \
    "$DIR_TEST/test_fp_cpp_client.cpp" "$DIR/impl.o" -lm $LDFLAGS -o "$DIR/link_finpred_cpp"
"$DIR/link_finpred_cpp"
echo 'C11, separate TU and C++17 linkage passed'
