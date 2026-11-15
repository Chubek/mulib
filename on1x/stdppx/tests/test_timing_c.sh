#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=${TMPDIR:-/tmp}/on1x-ppx-timing-test-$$
trap 'rm -f "$OUT" "$OUT.c"' EXIT HUP INT TERM
cat > "$OUT.c" <<'C'
#include <stdint.h>
#include "on1x_ppx_timing.h"
int main(void) {
  int64_t x = on1x_ppx_clock_ns();
  int64_t y = on1x_ppx_clock_ns();
  if (x <= 0 || y < x) return 1;
  on1x_ppx_timing_log("smoke", y-x);
  return 0;
}
C
"${CC:-cc}" -std=c11 -Wall -Wextra -Wpedantic -Werror \
 -I "$ROOT/runtime" "$ROOT/runtime/on1x_ppx_timing.c" "$OUT.c" -o "$OUT"
"$OUT"
