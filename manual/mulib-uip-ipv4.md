# mulib-uip-ipv4

**Build and parse verified unfragmented IPv4 packets and expose fragmentation flags**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_IPV4` once before including `mulib-uip-ipv4.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-uip-checksum.h`.

## Public API

```c
typedef struct mulib_uip_ipv4_packet {uint8_t src[4],dst[4],protocol,ttl;uint16_t id,fragment;const uint8_t *payload;size_t payload_length,header_length;} mulib_uip_ipv4_packet_t;
int mulib_uip_ipv4_parse(const void *packet,size_t length,mulib_uip_ipv4_packet_t *out);
int mulib_uip_ipv4_pack(void *out,size_t cap,const uint8_t src[4],const uint8_t dst[4],
                        uint8_t protocol,uint8_t ttl,uint16_t id,const void *payload,size_t n,size_t *used);
```

## Semantics and safety

IPv4 header checksum validated. Build no-option unfragmented DF packet, parse options (header length up to 60) but does not interpret options. Parser returns fragment flags for caller policy. Valid IP total length must fit available bytes; accepts trailing L2 padding.

## Demonstration

See `../tests/test_uip-ipv4.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-ipv4.h`](../include/mulib-uip-ipv4.h) |
| Implementation switch | `MULIB_IMPL_UIP_IPV4` |
| Repository-local includes | [mulib-uip-checksum](mulib-uip-checksum.md) |
| Complete example | [`examples/modules/uip-ipv4.c`](../examples/modules/uip-ipv4.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-ipv4.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
