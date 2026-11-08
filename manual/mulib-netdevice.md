# netdevice — interface address snapshots

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_NETDEVICE`; no dependencies.
See [`examples/modules/netdevice.c`](../examples/modules/netdevice.c).

On POSIX systems, `list` enumerates `getifaddrs` records. Each callback receives
the native interface name, index, flags, family, address and netmask. Family is
4 for IPv4, 6 for IPv6, or 0 for other records; non-IP address arrays are zero.
IPv6 scope ID is preserved. Multiple records may describe one interface and
interfaces may change during enumeration. Callback names/records are borrowed
only for the call. Return nonzero to stop; native snapshot storage is released
on completion or cancellation.

Results: 0 complete, 1 callback stopped, -1 invalid, -2 OS failure, -3
unsupported. `index(name)` resolves a native name to its index, or zero on
unknown/invalid/unsupported. No ownership is transferred. Flags are native
platform bits rather than a portable enum.

This is read-only discovery: address configuration, route changes, MTU changes
and link activation are external operations. Tests:
`tests/test_ipc_extensions.c` checks index consistency and callback cancellation.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-netdevice.h`](../include/mulib-netdevice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-netdevice.h
 * @brief POSIX network-interface enumeration and address snapshots.
 * Define MULIB_IMPL_NETDEVICE once. Returned callback records are borrowed.
 */
#ifndef MULIB_NETDEVICE_H
#define MULIB_NETDEVICE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_netdevice_info
  {
    const char *name;
    unsigned index, flags, family; /* family 0 (other), 4, 6 */
    unsigned char address[16], netmask[16];
    uint32_t scope;
  } mulib_netdevice_info_t;
  /** @brief Visit getifaddrs records (multiple per interface). 0 completion,
   * 1 callback stop, -1 invalid, -2 OS failure, -3 unsupported. */
  int mulib_netdevice_list (int (*visit) (void *,
                                          const mulib_netdevice_info_t *),
                            void *);
  /** @brief Resolve native interface index; zero if unknown/unsupported. */
  unsigned mulib_netdevice_index (const char *name);
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
| Header | [`include/mulib-netdevice.h`](../include/mulib-netdevice.h) |
| Implementation switch | `MULIB_IMPL_NETDEVICE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/netdevice.c`](../examples/modules/netdevice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netdevice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
