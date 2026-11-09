# mulib-net-monitor

**Subscribe to link, IPv4/IPv6 address and route multicast Netlink events**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NET_MONITOR` once before including `mulib-net-monitor.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`.

## Public API

```c
int mulib_net_monitor_open(mulib_netlink_socket_t *sock);
int mulib_net_monitor_read(mulib_netlink_socket_t *sock,void *buf,size_t cap,size_t *got);
```

## Semantics and safety

Read-only subscription, kernel frames returned raw. Watch for ENOBUFS and perform full resync; changes can occur faster than monitor consumption.

## Demonstration

See `../tests/test_net-monitor.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-net-monitor.h`](../include/mulib-net-monitor.h) |
| Implementation switch | `MULIB_IMPL_NET_MONITOR` |
| Repository-local includes | [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/net-monitor.c`](../examples/modules/net-monitor.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/net-monitor.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
