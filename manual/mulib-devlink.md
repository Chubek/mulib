# mulib-devlink

**Discover the Linux devlink Generic Netlink family ID**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DEVLINK` once before including `mulib-devlink.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink-genl.h`.

## Public API

```c
int mulib_devlink_family(uint16_t *family_id);
```

## Semantics and safety

Family discovery only. No flash/reset/reconfiguration operations.

## Demonstration

See `../tests/test_devlink.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-devlink.h`](../include/mulib-devlink.h) |
| Implementation switch | `MULIB_IMPL_DEVLINK` |
| Repository-local includes | [mulib-netlink-genl](mulib-netlink-genl.md) |
| Complete example | [`examples/modules/devlink.c`](../examples/modules/devlink.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/devlink.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
