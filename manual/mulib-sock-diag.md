# mulib-sock-diag

**Issue a bounded NETLINK_SOCK_DIAG inet socket dump request**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_SOCK_DIAG` once before including `mulib-sock-diag.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`.

## Public API

```c
int mulib_sock_diag_dump(unsigned family,unsigned protocol,mulib_netlink_visit_fn fn,void *ctx);
```

## Semantics and safety

IPv4/IPv6 TCP/UDP dumps. Returns raw inet_diag_msg replies through callback; excludes Unix domain socket dumps. Works without CAP_NET_ADMIN for sockets permitted to query.

## Demonstration

See `../tests/test_sock-diag.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sock-diag.h`](../include/mulib-sock-diag.h) |
| Implementation switch | `MULIB_IMPL_SOCK_DIAG` |
| Repository-local includes | [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/sock-diag.c`](../examples/modules/sock-diag.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sock-diag.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
