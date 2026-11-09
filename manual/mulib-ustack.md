# mulib-ustack

**Small userspace Ethernet/ARP/IPv4/ICMP echo/UDP endpoint with callback-driven I/O**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_USTACK` once before including `mulib-ustack.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-uip-ethernet.h`, `mulib-uip-arp.h`, `mulib-uip-ipv4.h`, `mulib-uip-icmp.h`, `mulib-uip-udp.h`, `mulib-uip-arp-cache.h`.

## Public API

```c
typedef int (*mulib_ustack_emit_fn)(const void *ethernet,size_t length,void *context);
typedef int (*mulib_ustack_udp_fn)(const uint8_t source[4],uint16_t src_port,uint16_t dst_port,
                                  const uint8_t *payload,size_t length,void *context);
typedef struct mulib_ustack {
 uint8_t mac[6],ip[4],netmask[4],gateway[4];
 mulib_uip_arp_cache_t arp;uint16_t next_id;uint64_t now;
 mulib_ustack_emit_fn emit;mulib_ustack_udp_fn udp;void *context;
} mulib_ustack_t;
int mulib_ustack_init(mulib_ustack_t *s,const uint8_t mac[6],const uint8_t ip[4],
                      const uint8_t mask[4],const uint8_t gateway[4],
                      mulib_ustack_emit_fn emit,mulib_ustack_udp_fn udp,void *ctx);
int mulib_ustack_process(mulib_ustack_t *s,const void *frame,size_t length);
int mulib_ustack_send_udp(mulib_ustack_t *s,const uint8_t dst[4],uint16_t src_port,
                          uint16_t dst_port,const void *payload,size_t length);
void mulib_ustack_tick(mulib_ustack_t *s,uint64_t now);
```

## Semantics and safety

Functional standalone Ethernet/ARP/IPv4/ICMPv4 echo/UDP endpoint. Explicit limitations: 1500-byte MTU, no IPv4 fragment handling in stack ingress, no IPv6 datapath, no TCP endpoints, no DHCP lease manager, no NAT/firewall, no raw TUN glue without adapter. Emits ARP request on unresolved UDP destination and returns -EAGAIN: caller retries after ARP reply. Not production hardened against network attackers; default ARP learning trusts replies, no ARP poisoning mitigation.

## Demonstration

See `../tests/test_ustack.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ustack.h`](../include/mulib-ustack.h) |
| Implementation switch | `MULIB_IMPL_USTACK` |
| Repository-local includes | [mulib-uip-arp-cache](mulib-uip-arp-cache.md), [mulib-uip-arp](mulib-uip-arp.md), [mulib-uip-ethernet](mulib-uip-ethernet.md), [mulib-uip-icmp](mulib-uip-icmp.md), [mulib-uip-ipv4](mulib-uip-ipv4.md), [mulib-uip-udp](mulib-uip-udp.md) |
| Complete example | [`examples/modules/ustack.c`](../examples/modules/ustack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ustack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
