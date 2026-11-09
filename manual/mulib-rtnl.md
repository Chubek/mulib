# mulib-rtnl

**NETLINK_ROUTE request builder and bounded synchronous kernel dumps**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_RTNL` once before including `mulib-rtnl.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`.

## Public API

```c
int mulib_rtnl_request(uint16_t msg_type,uint16_t flags,const void *body,size_t body_n,
                       const void *attrs,size_t attrs_n,mulib_netlink_visit_fn fn,void *ctx);
int mulib_rtnl_dump(uint16_t get_type,const void *body,size_t body_n,mulib_netlink_visit_fn fn,void *ctx);
int mulib_rtnl_link_kind(const void *nlmsg,size_t size,const char *kind);
```

## Semantics and safety

Linux NETLINK_ROUTE. One synchronous socket per request; no persistent cached state. Builder copies 16-KiB body and attributes, and supports bounded multipart dumps. Namespaced to calling thread. Request body must be correct for RTM message type.

## Demonstration

See `../tests/test_rtnl.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rtnl.h`](../include/mulib-rtnl.h) |
| Implementation switch | `MULIB_IMPL_RTNL` |
| Repository-local includes | [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/rtnl.c`](../examples/modules/rtnl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rtnl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
