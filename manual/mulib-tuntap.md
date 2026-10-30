# tuntap — Linux virtual packet interfaces

Implementation switches: `MULIB_IMPL_FILECTL`, `MULIB_IMPL_ABSTUN`,
`MULIB_IMPL_TUNTAP`. See
[`examples/modules/tuntap.c`](../examples/modules/tuntap.c).

Initialize with `MULIB_TUNTAP_INIT`. `open` opens `/dev/net/tun` nonblocking,
sets close-on-exec, and performs TUNSETIFF with IFF_NO_PI. `tap=0` selects TUN
(IP packets); `tap=1` selects TAP (Ethernet frames). Requested name is less
than 16 bytes; empty permits kernel selection. The resulting name is copied
into the handle. The configured packet maximum must be 1..65575 bytes and
include Ethernet headers for TAP. Opening may require native privileges.

The supplied maximum is a buffer contract, not an ioctl to set link MTU.
Configure link MTU, addresses, routes and activation externally, ensuring no
incoming packet exceeds this contract: native TUN reads discard an oversized
packet's remainder. No persistence, offloads, multi-queue or PI headers are
enabled. `backend` returns a borrowed `mulib_abstun_t`; the owning handle must
remain at the same address. Writes must complete a full packet.

Results: 0 success, 1 AGAIN for packet I/O, -1 invalid, -2 OS error, -3
unsupported. `close` resets ownership, never retries EINTR, and removes a
nonpersistent interface when its final descriptor closes. The automated example
and tests exercise unprivileged lifecycle/validation; actual interface creation
requires an appropriately configured Linux environment.
