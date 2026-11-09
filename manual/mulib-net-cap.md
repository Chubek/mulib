# mulib-net-cap

**Inspect effective Linux capability bits without changing credentials**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NET_CAP` once before including `mulib-net-cap.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_net_cap_effective(uint64_t *bits);
int mulib_net_cap_has(unsigned capability,int *yes);
```

## Semantics and safety

Reads CapEff in /proc/self/status, up to 64 bits. Does not authorize actions or change caps. Implementations must still respect kernel permission checks.

## Demonstration

See `../tests/test_net-cap.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-net-cap.h`](../include/mulib-net-cap.h) |
| Implementation switch | `MULIB_IMPL_NET_CAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/net-cap.c`](../examples/modules/net-cap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/net-cap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
