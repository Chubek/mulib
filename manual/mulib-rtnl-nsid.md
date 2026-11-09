# mulib-rtnl-nsid

**Dump kernel nsid objects via RTM_GETNSID**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_RTNL_NSID` once before including `mulib-rtnl-nsid.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-rtnl.h`.

## Public API

```c
int mulib_rtnl_nsid_dump(mulib_netlink_visit_fn fn,void *ctx);
```

## Semantics and safety

Read-only netlink dump; callback receives RTM_NEW* kernel reply. Requires kernel support for this request family; unsupported kernels return negative errno. Avoid assuming dump results are an atomic snapshot.

## Demonstration

See `../tests/test_rtnl-nsid.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rtnl-nsid.h`](../include/mulib-rtnl-nsid.h) |
| Implementation switch | `MULIB_IMPL_RTNL_NSID` |
| Repository-local includes | [mulib-rtnl](mulib-rtnl.md) |
| Complete example | [`examples/modules/rtnl-nsid.c`](../examples/modules/rtnl-nsid.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rtnl-nsid.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
