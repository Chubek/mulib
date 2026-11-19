#!/bin/sh
# Linux headless socket-to-render integration; only the wire fixture is required.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
CC=${CC:-cc}
PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$PARENT/mulib-wl-test.XXXXXX")
SERVER=
cleanup() {
    if [ -n "$SERVER" ]; then
        kill -TERM "$SERVER" 2>/dev/null || :
        wait "$SERVER" || :
    fi
    for file in wire server libwayland socket output.ppm xdg-shell.c xdg-shell-client-protocol.h; do
        rm -f -- "$BUILD/$file"
    done
    rmdir -- "$BUILD"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
FLAGS="-std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Werror -UNDEBUG ${CFLAGS:-}"
LINK="${LDFLAGS:-} -lm"
if [ "${SANITIZE:-0}" = 1 ]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
    LINK="$LINK -fsanitize=address,undefined"
fi
# Intentional word splitting for caller-supplied flags.
"$CC" $FLAGS -Iinclude tests/test-wl-headless-private.c \
    examples/wl-headless-private.c $LINK -o "$BUILD/wire"
TMPDIR="$PARENT" "$BUILD/wire"
"$CC" $FLAGS -Iinclude examples/wl-headless-private.c \
    examples/wl-headless-private-main.c $LINK -o "$BUILD/server"

if [ "${LIBWAYLAND:-1}" = 0 ] || ! command -v pkg-config >/dev/null 2>&1 ||
   ! command -v wayland-scanner >/dev/null 2>&1 || ! command -v timeout >/dev/null 2>&1 ||
   ! pkg-config --exists wayland-client wayland-protocols; then
    printf '%s\n' 'SKIP: optional libwayland-client fixture (disabled or dependencies unavailable)'
    exit 0
fi
XML="$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml"
wayland-scanner client-header "$XML" "$BUILD/xdg-shell-client-protocol.h"
wayland-scanner private-code "$XML" "$BUILD/xdg-shell.c"
"$CC" $FLAGS -I"$BUILD" $(pkg-config --cflags wayland-client) \
    tests/wl-headless-private-libwayland.c "$BUILD/xdg-shell.c" \
    $(pkg-config --libs wayland-client) $LINK -o "$BUILD/libwayland"
: > "$BUILD/socket"
TMPDIR="$PARENT" "$BUILD/server" "$BUILD/output.ppm" 15 > "$BUILD/socket" &
SERVER=$!
attempt=0
until read -r SOCKET < "$BUILD/socket" && [ -S "$SOCKET" ]; do
    kill -0 "$SERVER"
    attempt=$((attempt+1))
    [ "$attempt" -lt 100 ]
    sleep 0.02
done
timeout 8 "$BUILD/libwayland" "$SOCKET"
kill -TERM "$SERVER"
wait "$SERVER"
SERVER=
[ ! -e "$SOCKET" ]
[ ! -e "$(dirname -- "$SOCKET")" ]
set -- $(od -An -tu1 -j15 -N3 "$BUILD/output.ppm")
[ "$#" = 3 ]
[ "$1 $2 $3" = "68 136 204" ]
printf '%s\n' 'PASS: command-line PPM capture and SIGTERM removed private runtime directory'
