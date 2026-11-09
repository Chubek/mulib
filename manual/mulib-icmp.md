# mulib-icmp

**Compose and verify ICMP echo packets, with ones-complement checksum**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_ICMP` once before including `mulib-icmp.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
uint16_t mulib_icmp_checksum(const void *data,size_t length);
int mulib_icmp_echo(void *buffer,size_t cap,uint8_t type,uint16_t id,uint16_t sequence,
                    const void *payload,size_t payload_size,size_t *written);
```

## Semantics and safety

Raw ICMPv4 echo header/IPv4 checksum only. Type must be provided by caller (8 request, 0 response). For ICMPv6 include IPv6 pseudoheader checksum separately.

## Demonstration

See `../tests/test_icmp.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-icmp.h`](../include/mulib-icmp.h) |
| Implementation switch | `MULIB_IMPL_ICMP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/icmp.c`](../examples/modules/icmp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/icmp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
