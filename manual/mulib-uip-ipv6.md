# mulib-uip-ipv6

**Build and parse basic IPv6 fixed headers without extension-header processing**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_IPV6` once before including `mulib-uip-ipv6.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_uip_ipv6_packet {uint8_t src[16],dst[16],next_header,hop_limit;const uint8_t *payload;size_t payload_length;} mulib_uip_ipv6_packet_t;
int mulib_uip_ipv6_parse(const void *packet,size_t length,mulib_uip_ipv6_packet_t *out);
int mulib_uip_ipv6_pack(void *out,size_t cap,const uint8_t src[16],const uint8_t dst[16],
                        uint8_t next_header,uint8_t hops,const void *data,size_t n,size_t *used);
```

## Semantics and safety

Basic IPv6 header encoding/decoding only. No jumbo payload, extension headers, routing, NDP state machine, or ICMPv6 checksum. Packet length upper bounded at 65,535 bytes.

## Demonstration

See `../tests/test_uip-ipv6.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-ipv6.h`](../include/mulib-uip-ipv6.h) |
| Implementation switch | `MULIB_IMPL_UIP_IPV6` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-ipv6.c`](../examples/modules/uip-ipv6.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-ipv6.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
