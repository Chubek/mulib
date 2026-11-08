# tuntap — Linux virtual packet interfaces

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tuntap.h`](../include/mulib-tuntap.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-tuntap.h
 * @brief Linux TUN/TAP descriptors with no packet-information prefix.
 * Define MULIB_IMPL_TUNTAP, MULIB_IMPL_ABSTUN, MULIB_IMPL_FILECTL once.
 */
#ifndef MULIB_TUNTAP_H
#define MULIB_TUNTAP_H
#include "mulib-abstun.h"
#include "mulib-filectl.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_tuntap
  {
    int fd;
    size_t mtu;
    char name[16];
  } mulib_tuntap_t;
#define MULIB_TUNTAP_INIT { -1, 0, { 0 } }
  /** @brief Open /dev/net/tun, TUN if tap=0 or TAP if tap=1. requested is an
   * interface name or empty string for kernel selection. Nonblocking/cloexec.
   * mtu is maximum complete packet size (include Ethernet header for TAP).
   * Does not configure addresses/routes/link MTU. 0 success, -1 invalid,
   * -2 OS error (including permissions), -3 unsupported. */
  int mulib_tuntap_open (mulib_tuntap_t *, const char *requested, int tap,
                         size_t mtu);
  /** @brief Return a borrowed abstract tunnel bound to this live object. */
  mulib_abstun_t mulib_tuntap_backend (mulib_tuntap_t *);
  /** @brief Close/reset once. Kernel removes nonpersistent interfaces. */
  int mulib_tuntap_close (mulib_tuntap_t *);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-tuntap.h`](../include/mulib-tuntap.h) |
| Implementation switch | `MULIB_IMPL_TUNTAP` |
| Repository-local includes | [mulib-abstun](mulib-abstun.md), [mulib-filectl](mulib-filectl.md) |
| Complete example | [`examples/modules/tuntap.c`](../examples/modules/tuntap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tuntap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
