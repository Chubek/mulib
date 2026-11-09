# MuLIB networking modules

The 92 networking modules provide Linux administration APIs and userspace-IP
building blocks. Each module's `.md` documents its **implemented subset**, not
the whole Linux service or protocol with a similar name. See the [manual
index](index.md) for the complete MuLIB library inventory.

Define each required `MULIB_IMPL_<MODULE>` macro once in one C11 translation
unit. No third-party libraries are mandatory. These modules are registered in
the root `MANIFEST.yaml`, combined builds, and generated MuLL bridge.

## Userspace IP

`mulib-ustack.h` implements a small, actual callback-driven Ethernet/ARP/IPv4/ICMPv4 echo/UDP endpoint. IPv4 TCP is currently **only** a validated segment codec (`mulib-uip-tcp.h`) and an illustrative state transition helper (`mulib-uip-tcp-state.h`); it is NOT a complete TCP endpoint. IPv6 has a header codec only, not an IPv6 network stack. DHCPv4/DHCPv6 are message codecs and do not negotiate leases. No production security or congestion control is claimed.

For a real lwIP-level stack, subsequent work must implement: TCP transmission queue, retransmission timers, congestion control, SACK/windows, stream sockets, IPv6 NDP/SLAAC, persistent DHCP, DNS transport, routing/forwarding, and fuzz-hardening under adversarial traffic.

## Systems integration

Netlink: `netlink`, `netlink-attr`, `rtnl`, `rtnl-link/address/route/neighbor` provide concrete Linux kernel read and limited mutation. Read-only tools operate unprivileged; route/link mutators need CAP_NET_ADMIN. Many protocol-specific modules intentionally expose only a query or narrow interface.

D-Bus: `dbus-auth`, `dbus-transport`, `dbus-client` implement a small `AUTH EXTERNAL` UNIX-socket client and binary D-Bus framing with zero/one/two string arguments. NetworkManager/systemd/iwd adapters provide limited read-only service methods and usually return raw D-Bus reply frames. No a{sv} settings encoder, live daemon, or credential agent is implemented.

## Compile and test

```sh
./tests/run.sh
./examples/build.sh
./examples/build/udp-packet
```

Use a separate `BUILD_DIR` and `CC=clang CXX=clang++` to test with Clang.
Kernel modules target Linux; packet codecs use C11 and the clock helper uses
POSIX/Linux facilities. Strict C11 builds do not require `-D_GNU_SOURCE`.
CTest covers cross-TU C/C++ linkage. Privileged namespace and real-daemon
compatibility testing remain separate work; see [validation](VALIDATION.md).
