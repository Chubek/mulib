# mulib-uip-icmp

**Build verified ICMPv4 echo requests or replies for userspace IPv4**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_ICMP` once before including `mulib-uip-icmp.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: `mulib-uip-checksum.h`.

## Public API

```c
int mulib_uip_icmp_echo(void *out,size_t cap,int reply,uint16_t id,uint16_t sequence,
                        const void *data,size_t n,size_t *used);
int mulib_uip_icmp_verify(const void *pkt,size_t n);
```

## Semantics and safety

ICMPv4 echo only. No ICMPv6 pseudoheader checksum, ICMP errors, or rate limiting built in.

## Demonstration

See `../tests/test_uip-icmp.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-icmp.h`](../include/mulib-uip-icmp.h) |
| Implementation switch | `MULIB_IMPL_UIP_ICMP` |
| Repository-local includes | [mulib-uip-checksum](mulib-uip-checksum.md) |
| Complete example | [`examples/modules/uip-icmp.c`](../examples/modules/uip-icmp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-icmp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
