# mulib-uip-route

**Fixed-table longest-prefix matching for userspace IPv4 routing**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_ROUTE` once before including `mulib-uip-route.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_uip_route_entry {uint8_t prefix[4],gateway[4];uint8_t bits;unsigned interface_id;int valid;} mulib_uip_route_entry_t;
typedef struct mulib_uip_routes {mulib_uip_route_entry_t entries[16];} mulib_uip_routes_t;
int mulib_uip_route_add(mulib_uip_routes_t *table,const uint8_t prefix[4],unsigned bits,
                        const uint8_t gateway[4],unsigned ifid);
int mulib_uip_route_find(const mulib_uip_routes_t *table,const uint8_t address[4],mulib_uip_route_entry_t *out);
```

## Semantics and safety

Exactly 16 IPv4 route entries, longest prefix, deterministic first-entry tie-break. Does not perform source routing, ECMP, IPv6, VRF or policy rules.

## Demonstration

See `../tests/test_uip-route.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-route.h`](../include/mulib-uip-route.h) |
| Implementation switch | `MULIB_IMPL_UIP_ROUTE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-route.c`](../examples/modules/uip-route.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-route.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
