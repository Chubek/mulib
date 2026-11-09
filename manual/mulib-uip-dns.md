# mulib-uip-dns

**Build DNS A queries and decode one uncompressed or compressed A answer**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_DNS` once before including `mulib-uip-dns.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_uip_dns_query_a(const char *domain,uint16_t id,void *out,size_t capacity,size_t *written);
int mulib_uip_dns_first_a(const void *reply,size_t n,uint16_t expected_id,uint8_t ip[4]);
```

## Semantics and safety

Reply parsing rejects truncated (`TC`) replies and nonstandard opcodes. Name
compression pointers must point backward into the packet's name region, with
at most 128 decoding steps and 255 expanded wire bytes. Out-of-range pointers,
cycles, reserved label encodings and oversized names return `-EBADMSG`.
The API matches the ID but does not authenticate the source or verify that the
question/owner matches a requested domain; callers must enforce that policy.

DNS packet query builder and first A answer extractor, raw UDP or TCP transport supplied by caller. Compressed-name skip is bounded but does not resolve compression-pointer targets. No DNSSEC, CNAME recursion or retransmission policy.

## Demonstration

See `../tests/test_uip-dns.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-dns.h`](../include/mulib-uip-dns.h) |
| Implementation switch | `MULIB_IMPL_UIP_DNS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-dns.c`](../examples/modules/uip-dns.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-dns.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
