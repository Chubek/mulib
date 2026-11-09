# mulib-wifi-regulatory

**Request wireless regulatory information from nl80211 (NL80211_CMD_GET_REG)**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_WIFI_REGULATORY` once before including `mulib-wifi-regulatory.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-nl80211.h`.

## Public API

```c
int mulib_wifi_regulatory_query(mulib_netlink_visit_fn fn,void *ctx);
```

## Semantics and safety

Read-only nl80211 query. Reports kernel Generic Netlink message frames. Does not perform scanning triggers, AP creation, station association, or radio configuration; those are explicitly outside this bounded subset.

## Demonstration

See `../tests/test_wifi-regulatory.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-wifi-regulatory.h`](../include/mulib-wifi-regulatory.h) |
| Implementation switch | `MULIB_IMPL_WIFI_REGULATORY` |
| Repository-local includes | [mulib-nl80211](mulib-nl80211.md) |
| Complete example | [`examples/modules/wifi-regulatory.c`](../examples/modules/wifi-regulatory.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wifi-regulatory.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
