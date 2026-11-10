#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
CFLAGS="-std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror ${CFLAGS:-}"
TEMP_PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then TEMP_PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$TEMP_PARENT/mulib-sh-tests.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT INT TERM
for name in test_parallel test_parallel_extended test_parallel_hardening test_parallel_failures test_shebang test_shebang_hardening test_sh_alloc parallel shebang; do
    # shellcheck disable=SC2086
    "$CC" $CFLAGS "$ROOT/tests/$name.c" -o "$BUILD/$name" -pthread
done
"$BUILD/test_parallel" > "$BUILD/parallel.out"
"$BUILD/test_parallel_extended" > /dev/null
"$BUILD/test_shebang"
"$BUILD/test_parallel_hardening"
"$BUILD/test_parallel_failures"
"$BUILD/test_shebang_hardening"
"$BUILD/test_sh_alloc"
"$BUILD/parallel" -j 2 -k echo '{}' ::: 'red rose' blue > "$BUILD/cli.out"
printf 'red rose\nblue\n' > "$BUILD/expected"
cmp "$BUILD/cli.out" "$BUILD/expected"
if "$BUILD/parallel" -j 2 false ::: x y >/dev/null 2>&1; then
    echo 'ERROR: failing tasks reported success' >&2; exit 1
fi
cat > "$BUILD/sample.shb" << SCRIPT
#!$BUILD/shebang --filter=upper --define=WORD=hello
\${WORD} \${1}
SCRIPT
chmod +x "$BUILD/sample.shb"
"$BUILD/sample.shb" world > "$BUILD/shebang.out"
printf 'HELLO WORLD\n' > "$BUILD/expected"
cmp "$BUILD/shebang.out" "$BUILD/expected"
cat > "$BUILD/execute.shb" << SCRIPT
#!$BUILD/shebang --exec=/bin/sh --define=WORD=hello
printf '%s\\n' '\${WORD} world'
exit 7
SCRIPT
chmod +x "$BUILD/execute.shb"
status=0
"$BUILD/execute.shb" > "$BUILD/execute.out" 2>/dev/null || status=$?
test "$status" -eq 7
printf 'hello world\n' > "$BUILD/expected"
cmp "$BUILD/execute.out" "$BUILD/expected"
# Check declarations against the real public Mune header, then execute it.
# shellcheck disable=SC2086
"$CC" $CFLAGS -fsyntax-only "$ROOT/tests/test_mune_compile.c" -pthread
# shellcheck disable=SC2086
"$CC" $CFLAGS "$ROOT/tests/test_sh_mune.c" -o "$BUILD/test_sh_mune" -pthread -lm -ldl
"$BUILD/test_sh_mune"
sh "$ROOT/tests/test_sh_cli.sh" "$BUILD/parallel" "$BUILD/shebang" "$BUILD"
printf 'PASS: shell APIs, fault injection, kernel shebangs, CLI regressions, real Mune runtime\n'
