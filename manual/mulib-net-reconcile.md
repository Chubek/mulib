# mulib-net-reconcile

**Read-only desired-versus-observed interface activation comparison**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NET_RECONCILE` once before including `mulib-net-reconcile.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-net-profile.h`, `mulib-net-state.h`.

## Public API

```c
typedef struct mulib_net_reconcile_result {int interface_present;int oper_up;int differs;} mulib_net_reconcile_result_t;
int mulib_net_reconcile_check(const mulib_net_profile_t *desired,mulib_net_reconcile_result_t *out);
```

## Semantics and safety

Compares only interface presence and desired enabled flag against operational state. No mutation or IP address/gateway reconciliation; safe for unprivileged read-only monitoring.

## Demonstration

See `../tests/test_net-reconcile.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-net-reconcile.h`](../include/mulib-net-reconcile.h) |
| Implementation switch | `MULIB_IMPL_NET_RECONCILE` |
| Repository-local includes | [mulib-net-profile](mulib-net-profile.md), [mulib-net-state](mulib-net-state.md) |
| Complete example | [`examples/modules/net-reconcile.c`](../examples/modules/net-reconcile.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/net-reconcile.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
