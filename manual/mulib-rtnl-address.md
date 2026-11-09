# mulib-rtnl-address

**Enumerate or assign IPv4/IPv6 interface addresses over rtnetlink**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_RTNL_ADDRESS` once before including `mulib-rtnl-address.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-rtnl.h`, `mulib-netlink-attr.h`.

## Public API

```c
int mulib_rtnl_address_dump(unsigned family,mulib_netlink_visit_fn cb,void *ctx);
int mulib_rtnl_address_change(unsigned index,unsigned family,const void *address,unsigned prefix,int add);
```

## Semantics and safety

Address change accepts binary address bytes in network order, prefix and interface index. Requires CAP_NET_ADMIN. No lifetime/preferred-lifetime settings.

## Demonstration

See `../tests/test_rtnl-address.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rtnl-address.h`](../include/mulib-rtnl-address.h) |
| Implementation switch | `MULIB_IMPL_RTNL_ADDRESS` |
| Repository-local includes | [mulib-netlink-attr](mulib-netlink-attr.md), [mulib-rtnl](mulib-rtnl.md) |
| Complete example | [`examples/modules/rtnl-address.c`](../examples/modules/rtnl-address.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rtnl-address.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
