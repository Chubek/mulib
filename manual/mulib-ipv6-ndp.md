# mulib-ipv6-ndp

**Parse a bounded IPv6 Neighbor Solicitation or Advertisement header**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_IPV6_NDP` once before including `mulib-ipv6-ndp.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_ipv6_ndp {uint8_t type,code;uint32_t flags;uint8_t target[16];} mulib_ipv6_ndp_t;
int mulib_ipv6_ndp_parse(const void *icmp6,size_t length,mulib_ipv6_ndp_t *out);
```

## Semantics and safety

Parse NS/NA ICMPv6 body. No ICMPv6 pseudoheader checksum, option parsing, or full RFC4861 neighbor state machine.

## Demonstration

See `../tests/test_ipv6-ndp.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ipv6-ndp.h`](../include/mulib-ipv6-ndp.h) |
| Implementation switch | `MULIB_IMPL_IPV6_NDP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ipv6-ndp.c`](../examples/modules/ipv6-ndp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ipv6-ndp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
