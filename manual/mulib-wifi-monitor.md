# mulib-wifi-monitor

**Receive Linux RTMGRP_LINK notifications for wireless device state changes**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_WIFI_MONITOR` once before including `mulib-wifi-monitor.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`.

## Public API

```c
int mulib_wifi_monitor_open(mulib_netlink_socket_t *s);
int mulib_wifi_monitor_next(mulib_netlink_socket_t *s,void *buf,size_t cap,size_t *used);
```

## Semantics and safety

Subscribe to all link notifications, not nl80211 Wi-Fi association events. Application must inspect interface type; no Wi-Fi-only event filtering.

## Demonstration

See `../tests/test_wifi-monitor.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wifi-monitor.h`](../include/mulib-wifi-monitor.h) |
| Implementation switch | `MULIB_IMPL_WIFI_MONITOR` |
| Repository-local includes | [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/wifi-monitor.c`](../examples/modules/wifi-monitor.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wifi-monitor.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
