# mulib-netfilter-log

**Subscribe to a Linux NFLOG group with a raw NETLINK_NETFILTER socket**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETFILTER_LOG` once before including `mulib-netfilter-log.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-nfnetlink.h`, `mulib-netlink-attr.h`.

## Public API

```c
int mulib_netfilter_log_subscribe(mulib_netlink_socket_t *s,uint16_t group);
```

## Semantics and safety

NFLOG bind subset. Caller receives events through netlink socket and should implement kernel per-group configuration and asynchronous event parsing. Requires appropriate permissions.

## Demonstration

See `../tests/test_netfilter-log.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netfilter-log.h`](../include/mulib-netfilter-log.h) |
| Implementation switch | `MULIB_IMPL_NETFILTER_LOG` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md), [mulib-nfnetlink](mulib-nfnetlink.md) |
| Complete example | [`examples/modules/netfilter-log.c`](../examples/modules/netfilter-log.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netfilter-log.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
