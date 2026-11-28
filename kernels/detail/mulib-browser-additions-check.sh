#!/bin/sh
# Run from any directory; compile only this isolated staging tree.
set -eu
K=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT HUP INT TERM
CC=${CC:-cc}
for h in "$K"/mulib-*.h; do
  name=$(basename "$h")
  printf '#include "%s"\nint main(void){return 0;}\n' "$name" | "$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$K" -x c -fsyntax-only -
  printf '#include "%s"\n' "$name" >> "$T/all.c"
done
printf 'int main(void){return 0;}\n' >> "$T/all.c"
"$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$K" "$T/all.c" -lm -o "$T/all"
"$T/all"
"$CC" -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$K" "$K/detail/mulib-browser-additions-smoke.c" -lm -o "$T/smoke"
"$T/smoke"
if command -v clang >/dev/null 2>&1; then
  clang -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$K" "$T/all.c" -lm -o "$T/all-clang"
  "$T/all-clang"
  clang -std=c11 -Wall -Wextra -Wpedantic -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I"$K" "$K/detail/mulib-browser-additions-smoke.c" -lm -o "$T/smoke-san"
  ASAN_OPTIONS=detect_leaks=1 "$T/smoke-san"
fi
printf '%s\n' 'Staging header and behavioral checks passed.'
