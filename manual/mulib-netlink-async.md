# mulib-netlink-async

**poll-based bounded readiness and a single pending request completion primitive**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_NETLINK_ASYNC` once before including `mulib-netlink-async.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-netlink.h`.

## Public API

```c
int mulib_netlink_async_wait(int fd,int milliseconds,int writable);
int mulib_netlink_async_recv(mulib_netlink_socket_t *s,int milliseconds,void *buf,size_t cap,size_t *used);
```

## Semantics and safety

Waits propagate `-EINTR` rather than restarting the whole timeout. Invalid
descriptors return `-EBADF`; a hangup without the requested readiness (or any
hangup when waiting to write) returns `-EPIPE`. Readable EOF is reported as
readiness so the receive operation can diagnose it. The receive helper validates
its buffers before waiting and clears `used` on a valid call, including timeout.

One fd readiness poll; not a full concurrent request scheduler. Return -ETIMEDOUT on timeout. Consumers implement correlation by nlmsg_seq.

## Demonstration

See `../tests/test_netlink-async.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-netlink-async.h`](../include/mulib-netlink-async.h) |
| Implementation switch | `MULIB_IMPL_NETLINK_ASYNC` |
| Repository-local includes | [mulib-netlink](mulib-netlink.md) |
| Complete example | [`examples/modules/netlink-async.c`](../examples/modules/netlink-async.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/netlink-async.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
