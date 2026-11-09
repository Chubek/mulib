# mulib-dhcp4-client

**Build DHCPv4 DISCOVER messages and decode DHCP message type options**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_DHCP4_CLIENT` once before including `mulib-dhcp4-client.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
int mulib_dhcp4_client_discover(void *out,size_t cap,const uint8_t mac[6],uint32_t xid,size_t *written);
int mulib_dhcp4_client_type(const void *packet,size_t size,uint8_t *type);
```

## Semantics and safety

DHCPv4 datagram codec only (DISCOVER+message-type parsing); no socket lifecycle, lease negotiation, renewal, conflict detection, persistence or IP configuration. Supplied MAC and transaction ID are caller-owned. Do not imply full DHCP client interoperability.

## Demonstration

See `../tests/test_dhcp4-client.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dhcp4-client.h`](../include/mulib-dhcp4-client.h) |
| Implementation switch | `MULIB_IMPL_DHCP4_CLIENT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dhcp4-client.c`](../examples/modules/dhcp4-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dhcp4-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
