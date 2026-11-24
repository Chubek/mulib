#!/bin/sh
# Explicit SDK-enabled test; no display, DRM ownership or link to libvulkan.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
TEMP_PARENT=${TMPDIR:-/tmp}
if [ -z "${TMPDIR:-}" ] && [ -d /tmp/opencode ]; then TEMP_PARENT=/tmp/opencode; fi
BUILD=$(mktemp -d "$TEMP_PARENT/mulib-vulkan.XXXXXX")
trap 'rm -f "$BUILD/legacy" "$BUILD/device"; rmdir "$BUILD"' EXIT
trap 'exit 1' HUP INT TERM
FLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror ${CFLAGS:-}"
LINK="${LDFLAGS:-} -ldl"
if [ "${SANITIZE:-0}" = 1 ]; then
    FLAGS="$FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
    LINK="$LINK -fsanitize=address,undefined"
fi
# Deliberate word splitting of compiler and linker flags.
# shellcheck disable=SC2086
"$CC" $FLAGS ${CPPFLAGS:-} -I"$ROOT/kernels" "$ROOT/tests/graphics/vulkan-legacy.c" $LINK -o "$BUILD/legacy"
# shellcheck disable=SC2086
"$CC" $FLAGS ${CPPFLAGS:-} -I"$ROOT/kernels" -DMULIB_GLX_VULKAN_ENABLE \
    "$ROOT/tests/graphics/vulkan-device.c" $LINK -o "$BUILD/device"
"$BUILD/legacy"
"$BUILD/device"
