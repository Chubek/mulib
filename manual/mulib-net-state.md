# mulib-net-state

**Snapshot interface state from sysfs without modifying the network**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NET_STATE` once before including `mulib-net-state.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_net_state {unsigned index;uint32_t mtu;int oper_up;} mulib_net_state_t;
int mulib_net_state_read(const char *interface,mulib_net_state_t *out);
```

## Semantics and safety

Bounded sysfs interface state read. oper_up means operstate == up (not merely IFF_UP). No coherent atomic snapshot.

## Demonstration

See `../tests/test_net-state.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-net-state.h`](../include/mulib-net-state.h) |
| Implementation switch | `MULIB_IMPL_NET_STATE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/net-state.c`](../examples/modules/net-state.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/net-state.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
