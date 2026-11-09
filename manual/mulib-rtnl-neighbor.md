# mulib-rtnl-neighbor

**Dump kernel ARP/NDP neighbor entries and add or remove static neighbors**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_RTNL_NEIGHBOR` once before including `mulib-rtnl-neighbor.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-rtnl.h`, `mulib-netlink-attr.h`.

## Public API

```c
int mulib_rtnl_neighbor_dump(unsigned family,mulib_netlink_visit_fn fn,void *ctx);
int mulib_rtnl_neighbor_change(unsigned ifindex,unsigned family,const void *ip,
                               const uint8_t mac[6],int add);
```

## Semantics and safety

Static IPv4/IPv6 neighbor subset, six-byte L2 address. Dynamic neighbor cache remains kernel-owned.

## Demonstration

See `../tests/test_rtnl-neighbor.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rtnl-neighbor.h`](../include/mulib-rtnl-neighbor.h) |
| Implementation switch | `MULIB_IMPL_RTNL_NEIGHBOR` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md), [mulib-rtnl](mulib-rtnl.md) |
| Complete example | [`examples/modules/rtnl-neighbor.c`](../examples/modules/rtnl-neighbor.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rtnl-neighbor.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
