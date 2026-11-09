# mulib-inet-diag

**Decode a Linux inet_diag_msg reply into a concise socket record**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_INET_DIAG` once before including `mulib-inet-diag.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-sock-diag.h`.

## Public API

```c
typedef struct mulib_inet_diag_socket {unsigned family,state;uint16_t src_port,dst_port;uint32_t uid,inode;uint8_t src[16],dst[16];} mulib_inet_diag_socket_t;
int mulib_inet_diag_decode(const void *nlmsg,size_t n,mulib_inet_diag_socket_t *out);
```

## Semantics and safety

Only standard inet_diag_msg prefix, validates NLMSG length. IPv4/IPv6 addresses returned in network byte order.

Unaligned byte buffers are accepted. The declared message length must include
the entire `inet_diag_msg`, even if the backing buffer is larger. Unknown
address families return `-EAFNOSUPPORT`; no partial output is published.

## Demonstration

See `../tests/test_inet-diag.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-inet-diag.h`](../include/mulib-inet-diag.h) |
| Implementation switch | `MULIB_IMPL_INET_DIAG` |
| Repository-local includes | [mulib-sock-diag](mulib-sock-diag.md) |
| Complete example | [`examples/modules/inet-diag.c`](../examples/modules/inet-diag.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/inet-diag.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
