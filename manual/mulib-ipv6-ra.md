# mulib-ipv6-ra

**Parse ICMPv6 Router Advertisement header and a prefix-information option**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_IPV6_RA` once before including `mulib-ipv6-ra.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_ipv6_ra {uint8_t hop_limit,flags;uint16_t lifetime;} mulib_ipv6_ra_t;
int mulib_ipv6_ra_parse(const void *icmp6,size_t size,mulib_ipv6_ra_t *out);
int mulib_ipv6_ra_prefix(const void *icmp6,size_t size,uint8_t prefix[16],unsigned *prefix_bits);
```

## Semantics and safety

ICMPv6 Router Advertisement and first prefix information option only. Does not validate source hop limit 255/checksum or configure SLAAC. Caller must verify RA origin and security policy.

## Demonstration

See `../tests/test_ipv6-ra.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ipv6-ra.h`](../include/mulib-ipv6-ra.h) |
| Implementation switch | `MULIB_IMPL_IPV6_RA` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ipv6-ra.c`](../examples/modules/ipv6-ra.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ipv6-ra.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
