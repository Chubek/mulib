#!/bin/sh
# Deterministic fixtures, plus optional real Xvfb integration. No use of DISPLAY.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
XVFB=${XVFB:-Xvfb}
TEMP_PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then TEMP_PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$TEMP_PARENT/mulib-x11-nested.XXXXXX")
server_pid=
wm_pid=
cleanup() {
    if [ -n "$wm_pid" ]; then kill "$wm_pid" 2>/dev/null || :; wait "$wm_pid" 2>/dev/null || :; fi
    if [ -n "$server_pid" ]; then kill "$server_pid" 2>/dev/null || :; wait "$server_pid" 2>/dev/null || :; fi
    rm -f -- "$BUILD"/wire "$BUILD"/live "$BUILD"/wm "$BUILD"/server.log \
        "$BUILD"/wm.log "$BUILD"/second.log "$BUILD"/display "$BUILD"/cookie \
        "$BUILD"/authority "$BUILD"/authority-c "$BUILD"/authority-l
    rmdir -- "$BUILD"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
umask 077

compile() {
    # Deliberate word splitting for compiler/linker flags.
    "$CC" -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Werror \
        ${CFLAGS:-} -UNDEBUG -I"$ROOT/include" "$@" ${LDFLAGS:-}
}
compile "$ROOT/tests/test-x11-nested-wire.c" -o "$BUILD/wire"
compile "$ROOT/examples/x11-nested-wm.c" -o "$BUILD/wm"
"$BUILD/wire"
if ! command -v "$XVFB" >/dev/null 2>&1 || ! command -v xauth >/dev/null 2>&1 ||
   ! command -v timeout >/dev/null 2>&1 || ! command -v pkg-config >/dev/null 2>&1 ||
   ! pkg-config --exists xcb; then
    printf 'SKIP: live X11 (requires Xvfb, xauth, timeout, and xcb development files)\n'
    exit 0
fi

compile "$ROOT/tests/test-x11-nested-live.c" $(pkg-config --cflags --libs xcb) -o "$BUILD/live"
dd if=/dev/urandom of="$BUILD/cookie" bs=16 count=1 status=none
cookie_hex=$(od -An -tx1 -v "$BUILD/cookie" | tr -d ' \n')
: > "$BUILD/authority"
# Xvfb loads the cookie from this private file. The explicit UNIX socket and
# raw cookie are supplied to the test clients; their authority lookup is unused.
printf 'add :0 MIT-MAGIC-COOKIE-1 %s\n' "$cookie_hex" | xauth -q -f "$BUILD/authority" source -
"$XVFB" -displayfd 3 -screen 0 640x480x24 -nolisten tcp -noreset \
    -auth "$BUILD/authority" 3>"$BUILD/display" >"$BUILD/server.log" 2>&1 &
server_pid=$!
i=0
while [ ! -s "$BUILD/display" ]; do
    i=$((i + 1))
    if [ "$i" -ge 100 ] || ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$BUILD/server.log" >&2
        exit 1
    fi
    sleep 0.05
done
IFS= read -r display < "$BUILD/display"
case "$display" in
    ''|*[!0-9]*) printf 'Invalid Xvfb display number\n' >&2; exit 1 ;;
esac
socket="/tmp/.X11-unix/X$display"
timeout 15 "$BUILD/live" "$socket" "$BUILD/cookie"

"$BUILD/wm" --private-socket "$socket" "$BUILD/cookie" 2>"$BUILD/wm.log" &
wm_pid=$!
i=0
while ! grep -q '^managing private X11 root ' "$BUILD/wm.log"; do
    i=$((i + 1))
    if [ "$i" -ge 100 ] || ! kill -0 "$wm_pid" 2>/dev/null; then
        cat "$BUILD/wm.log" >&2
        exit 1
    fi
    sleep 0.05
done
status=0
timeout 10 "$BUILD/wm" --private-socket "$socket" "$BUILD/cookie" 2>"$BUILD/second.log" || status=$?
if [ "$status" -ne 3 ] || ! grep -q 'BadAccess' "$BUILD/second.log"; then
    cat "$BUILD/second.log" >&2
    exit 1
fi
printf 'X11 nested WM example and second-manager rejection: PASS\n'
