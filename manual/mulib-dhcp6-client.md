# mulib-dhcp6-client

**Build a DHCPv6 SOLICIT and parse a DHCPv6 option TLV**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DHCP6_CLIENT` once before including `mulib-dhcp6-client.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_dhcp6_client_solicit(void *buf,size_t cap,uint32_t transaction,size_t *written);
int mulib_dhcp6_client_option(const void *pkt,size_t size,uint16_t kind,
                               const uint8_t **value,size_t *length);
```

## Semantics and safety

DHCPv6 message builder and option decoder only. The 4-byte SOLICIT omits mandatory client identifier for normal deployments; callers must append DUID/IA_NA/ORO. No DHCPv6 client state machine.

## Demonstration

See `../tests/test_dhcp6-client.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dhcp6-client.h`](../include/mulib-dhcp6-client.h) |
| Implementation switch | `MULIB_IMPL_DHCP6_CLIENT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dhcp6-client.c`](../examples/modules/dhcp6-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dhcp6-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
