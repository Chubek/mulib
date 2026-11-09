# mulib-uip-udp

**Build/check IPv4 UDP datagrams with correct pseudoheader checksums**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_UDP` once before including `mulib-uip-udp.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-uip-checksum.h`.

## Public API

```c
typedef struct mulib_uip_udp_datagram {uint16_t src_port,dst_port;const uint8_t *payload;size_t length;} mulib_uip_udp_datagram_t;
int mulib_uip_udp_parse(const uint8_t src_ip[4],const uint8_t dst_ip[4],const void *pkt,size_t n,mulib_uip_udp_datagram_t *out);
int mulib_uip_udp_pack(void *pkt,size_t cap,const uint8_t src_ip[4],const uint8_t dst_ip[4],
                       uint16_t src_port,uint16_t dst_port,const void *payload,size_t n,size_t *used);
```

## Semantics and safety

IPv4 UDP only. Checksum zero is accepted on IPv4 inbound. No fragment reassembly or UDP socket abstraction; caller supplies source/destination addresses.

## Demonstration

See `../tests/test_uip-udp.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-udp.h`](../include/mulib-uip-udp.h) |
| Implementation switch | `MULIB_IMPL_UIP_UDP` |
| Repository-local includes | [mulib-uip-checksum](mulib-uip-checksum.md) |
| Complete example | [`examples/modules/uip-udp.c`](../examples/modules/uip-udp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-udp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
