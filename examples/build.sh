#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p examples/build
CC=${CC:-cc}
# These are the networking demos; unrelated examples have their own CMake
# targets and may need different fixtures or compiler options.
for name in dhcp4-discover dns-query generic-netlink icmp-echo ip-links \
    network-profile nm-state pcap-write show-route socket-diagnostics \
    udp-packet ustack-demo; do
    src="examples/$name.c"
    "$CC" ${CFLAGS:-} -std=c11 -pthread -O1 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude "$src" -lm -ldl -o "examples/build/$(basename "$src" .c)"
done
printf 'Compiled examples into examples/build/\n'
