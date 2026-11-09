# mulib-rtnl-link

**Inspect and administratively bring interfaces up/down via RTM_NEWLINK**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_RTNL_LINK` once before including `mulib-rtnl-link.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-rtnl.h`.

## Public API

```c
int mulib_rtnl_link_index(const char *name);
int mulib_rtnl_link_set_up(unsigned index,int up);
int mulib_rtnl_link_dump(mulib_netlink_visit_fn fn,void *ctx);
```

## Semantics and safety

One permission-checked mutation API (UP/DOWN), not a complete netlink link parameter editor. Dumps return validated kernel RTM_NEWLINK frames.

## Demonstration

See `../tests/test_rtnl-link.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rtnl-link.h`](../include/mulib-rtnl-link.h) |
| Implementation switch | `MULIB_IMPL_RTNL_LINK` |
| Repository-local includes | [mulib-rtnl](mulib-rtnl.md) |
| Complete example | [`examples/modules/rtnl-link.c`](../examples/modules/rtnl-link.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rtnl-link.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
