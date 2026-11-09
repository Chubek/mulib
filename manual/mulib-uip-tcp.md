# mulib-uip-tcp

**Parse basic TCP headers and encode TCP no-option IPv4 segments with checksum**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_TCP` once before including `mulib-uip-tcp.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-uip-checksum.h`.

## Public API

```c
typedef struct mulib_uip_tcp_segment {uint16_t source_port,dest_port,window;uint32_t sequence,acknowledgment;uint8_t flags;const uint8_t *data;size_t length;} mulib_uip_tcp_segment_t;
int mulib_uip_tcp_parse(const uint8_t src[4],const uint8_t dst[4],const void *packet,size_t n,mulib_uip_tcp_segment_t *out);
int mulib_uip_tcp_pack(void *out,size_t capacity,const uint8_t src[4],const uint8_t dst[4],
                       const mulib_uip_tcp_segment_t *segment,size_t *used);
```

## Semantics and safety

TCP segment codec only, not a TCP stack: no retransmission, congestion/window control, SACK, options, sequence wrap handling, connection table or timers. Input checksum validated.

## Demonstration

See `../tests/test_uip-tcp.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-tcp.h`](../include/mulib-uip-tcp.h) |
| Implementation switch | `MULIB_IMPL_UIP_TCP` |
| Repository-local includes | [mulib-uip-checksum](mulib-uip-checksum.md) |
| Complete example | [`examples/modules/uip-tcp.c`](../examples/modules/uip-tcp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-tcp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
