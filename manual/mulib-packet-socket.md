# mulib-packet-socket

**Open, bind, and receive Linux AF_PACKET raw Ethernet frames**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_PACKET_SOCKET` once before including `mulib-packet-socket.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_packet_socket_open(unsigned ifindex,uint16_t ethertype);
int mulib_packet_socket_receive(int fd,void *buffer,size_t capacity,size_t *received);
```

## Semantics and safety

Caller closes returned fd. AF_PACKET requires CAP_NET_RAW. Truncated frames discarded. No packet-ring zero-copy implementation.

## Demonstration

See `../tests/test_packet-socket.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-packet-socket.h`](../include/mulib-packet-socket.h) |
| Implementation switch | `MULIB_IMPL_PACKET_SOCKET` |
| Repository-local includes | None |
| Complete example | [`examples/modules/packet-socket.c`](../examples/modules/packet-socket.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/packet-socket.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
