# mulib-uip-checksum

**RFC 1071 Internet checksum and IPv4 TCP/UDP pseudoheader checksum**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_CHECKSUM` once before including `mulib-uip-checksum.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
uint16_t mulib_uip_checksum(const void *data,size_t size);
uint16_t mulib_uip_checksum_ipv4(const uint8_t src[4],const uint8_t dst[4],uint8_t proto,const void *segment,size_t size);
```

## Semantics and safety

Network-order ones-complement checksum. Output 0 means valid checksum when validating a complete header/segment. For a zero UDP checksum, emit 0xffff on the wire; caller handles.

The generic checksum folds carries throughout accumulation, including buffers
larger than an IP packet. NULL is allowed only with zero length. The IPv4
pseudoheader helper requires both four-byte addresses and a segment of at most
65535 bytes. Invalid arguments return zero, which is not an error-discriminating
result: callers must validate those preconditions before treating zero as a
successful checksum verification.

## Demonstration

See `../tests/test_uip-checksum.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-checksum.h`](../include/mulib-uip-checksum.h) |
| Implementation switch | `MULIB_IMPL_UIP_CHECKSUM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-checksum.c`](../examples/modules/uip-checksum.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-checksum.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
