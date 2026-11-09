# mulib-uip-ethernet

**Serialize and validate fixed-length Ethernet II frames**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_ETHERNET` once before including `mulib-uip-ethernet.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_uip_ethernet_frame {uint8_t dst[6],src[6];uint16_t ethertype;const uint8_t *payload;size_t length;} mulib_uip_ethernet_frame_t;
int mulib_uip_ethernet_parse(const void *frame,size_t length,mulib_uip_ethernet_frame_t *out);
int mulib_uip_ethernet_pack(void *frame,size_t cap,const uint8_t dst[6],const uint8_t src[6],
                            uint16_t ethertype,const void *payload,size_t n,size_t *used);
```

## Semantics and safety

Ethernet II header only. VLAN tagged frames and FCS excluded. Caller may pad to Ethernet minimum frame length; TAP generally handles frame size appropriate to interface.

## Demonstration

See `../tests/test_uip-ethernet.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-ethernet.h`](../include/mulib-uip-ethernet.h) |
| Implementation switch | `MULIB_IMPL_UIP_ETHERNET` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-ethernet.c`](../examples/modules/uip-ethernet.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-ethernet.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
