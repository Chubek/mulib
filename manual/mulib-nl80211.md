# mulib-nl80211

**Generic Netlink nl80211 request helper for wireless devices**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NL80211` once before including `mulib-nl80211.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`, `mulib-netlink-genl.h`, `mulib-netlink-attr.h`.

## Public API

```c
int mulib_nl80211_command(uint8_t command,unsigned ifindex,unsigned flags,
                          mulib_netlink_visit_fn visit,void *ctx);
```

## Semantics and safety

Kernel nl80211 command request. No silent Wi-Fi association or credential handling. Requires compatible Linux kernel and driver. Caller interprets raw Generic Netlink replies.

## Demonstration

See `../tests/test_nl80211.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-nl80211.h`](../include/mulib-nl80211.h) |
| Implementation switch | `MULIB_IMPL_NL80211` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md), [mulib-netlink-genl](mulib-netlink-genl.md), [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/nl80211.c`](../examples/modules/nl80211.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/nl80211.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
