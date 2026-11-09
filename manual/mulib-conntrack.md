# mulib-conntrack

**Dump conntrack table entries through netfilter conntrack Netlink subsystem**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_CONNTRACK` once before including `mulib-conntrack.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-nfnetlink.h`.

## Public API

```c
int mulib_conntrack_dump(unsigned family,mulib_netlink_visit_fn fn,void *ctx);
```

## Semantics and safety

Read-only conntrack table dump. Raw frames contain nested nfattrs, inspected by caller. Restricted by kernel permissions.

## Demonstration

See `../tests/test_conntrack.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-conntrack.h`](../include/mulib-conntrack.h) |
| Implementation switch | `MULIB_IMPL_CONNTRACK` |
| Repository-local includes | [mulib-nfnetlink](mulib-nfnetlink.md) |
| Complete example | [`examples/modules/conntrack.c`](../examples/modules/conntrack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/conntrack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
