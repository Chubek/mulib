# mulib-netlink-genl

**Resolve Generic Netlink family IDs through the kernel control family**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETLINK_GENL` once before including `mulib-netlink-genl.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`, `mulib-netlink-attr.h`.

## Public API

```c
int mulib_netlink_genl_resolve(mulib_netlink_socket_t *s,const char *family,uint16_t *out);
```

## Semantics and safety

Linux only; query genl controller by family name, returning numeric family ID. Caller must open a NETLINK_GENERIC socket. Does not discover multicast group IDs.

Family names must contain 1..15 bytes, excluding the terminator. Family IDs
must have exactly a two-byte payload and occur once; malformed/duplicate IDs
return `-EBADMSG`. The output is unchanged if resolution fails.

## Demonstration

See `../tests/test_netlink-genl.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netlink-genl.h`](../include/mulib-netlink-genl.h) |
| Implementation switch | `MULIB_IMPL_NETLINK_GENL` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md), [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/netlink-genl.c`](../examples/modules/netlink-genl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netlink-genl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
