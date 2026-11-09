# mulib-netlink

**Linux NETLINK_ROUTE/Generic Netlink socket, bounded request/response transactions and message iteration**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETLINK` once before including `mulib-netlink.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_netlink_socket { int fd; uint32_t pid, seq; int protocol; } mulib_netlink_socket_t;
#define MULIB_NETLINK_INIT { -1, 0, 0, 0 }
typedef int (*mulib_netlink_visit_fn)(const void *message, size_t length, void *ctx);
int mulib_netlink_open(mulib_netlink_socket_t *s, int protocol, unsigned groups);
int mulib_netlink_close(mulib_netlink_socket_t *s);
int mulib_netlink_send(mulib_netlink_socket_t *s, void *msg, size_t n);
int mulib_netlink_receive(mulib_netlink_socket_t *s, void *out, size_t cap, size_t *got);
int mulib_netlink_transact(mulib_netlink_socket_t *s, void *msg, size_t n,
                            mulib_netlink_visit_fn visit, void *ctx);
int mulib_netlink_foreach(const void *data, size_t n, mulib_netlink_visit_fn visit, void *ctx);
```

## Semantics and safety

Byte-oriented iterators and request submission accept unaligned buffers.
Transaction callbacks receive borrowed messages valid only during the call.
Interrupted dumps (`NLM_F_DUMP_INTR`) fail with `-EINTR` and overruns with
`-ENOBUFS`; any records already visited are an incomplete snapshot and must be
discarded. Sequence zero is reserved for notifications. Positive wire error
codes and malformed completion payloads are rejected. `NLMSG_NOOP` does not
complete a request. A callback error stops the transaction; close/reopen before
reusing a socket with unread replies.

Linux only. Return 0 on success or negative errno. `recvmsg` requires a kernel
sender and rejects truncation. Transactions use a 64-KiB receive buffer, at most
1024 datagrams, and the socket's two-second **per-read** timeout; this is not a
two-second overall deadline. Direct receives use the caller's capacity.
The transaction caller owns the mutable request buffer; synchronous transactions
must not run concurrently on the same socket. Multicast use requires a separate
event socket. Buffer lengths must describe actual accessible storage.

## Demonstration

See `../tests/test_netlink.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netlink.h`](../include/mulib-netlink.h) |
| Implementation switch | `MULIB_IMPL_NETLINK` |
| Repository-local includes | None |
| Complete example | [`examples/modules/netlink.c`](../examples/modules/netlink.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netlink.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
