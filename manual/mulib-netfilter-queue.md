# mulib-netfilter-queue

**Construct NFQUEUE queue-bind commands and open a bound Netfilter queue socket**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETFILTER_QUEUE` once before including `mulib-netfilter-queue.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-nfnetlink.h`, `mulib-netlink-attr.h`.

## Public API

```c
int mulib_netfilter_queue_bind(mulib_netlink_socket_t *s,uint16_t queue_id);
```

## Semantics and safety

Bind a queue to a socket. Caller must consume queue messages and issue verdicts to avoid traffic stalls; this subset does NOT provide verdict APIs. Do not attach a live firewall rule directing production traffic to this queue.

## Demonstration

See `../tests/test_netfilter-queue.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netfilter-queue.h`](../include/mulib-netfilter-queue.h) |
| Implementation switch | `MULIB_IMPL_NETFILTER_QUEUE` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md), [mulib-nfnetlink](mulib-nfnetlink.md) |
| Complete example | [`examples/modules/netfilter-queue.c`](../examples/modules/netfilter-queue.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netfilter-queue.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
