# mulib-rtnl-vrf

**Discover Linux vrf link-kind interfaces using RTM_GETLINK linkinfo filtering**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_RTNL_VRF` once before including `mulib-rtnl-vrf.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-rtnl.h`, `mulib-rtnl-link.h`.

## Public API

```c
int mulib_rtnl_vrf_dump(mulib_netlink_visit_fn fn,void *ctx);
```

## Semantics and safety

Read-only list of RTM_NEWLINK frames with matching IFLA_INFO_KIND; no create/delete operations. Filtering is strict and bounded. Works in the calling network namespace.

## Demonstration

See `../tests/test_rtnl-vrf.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rtnl-vrf.h`](../include/mulib-rtnl-vrf.h) |
| Implementation switch | `MULIB_IMPL_RTNL_VRF` |
| Repository-local includes | [mulib-rtnl-link](mulib-rtnl-link.md), [mulib-rtnl](mulib-rtnl.md) |
| Complete example | [`examples/modules/rtnl-vrf.c`](../examples/modules/rtnl-vrf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rtnl-vrf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
