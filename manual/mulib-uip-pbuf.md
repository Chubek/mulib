# mulib-uip-pbuf

**Owned dynamically allocated packet buffer supporting prepend and append**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_PBUF` once before including `mulib-uip-pbuf.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_uip_pbuf {uint8_t *memory;size_t capacity,start,length;} mulib_uip_pbuf_t;
int mulib_uip_pbuf_init(mulib_uip_pbuf_t *p,size_t headroom,size_t capacity);
void mulib_uip_pbuf_destroy(mulib_uip_pbuf_t *p);
int mulib_uip_pbuf_prepend(mulib_uip_pbuf_t *p,const void *data,size_t n);
int mulib_uip_pbuf_append(mulib_uip_pbuf_t *p,const void *data,size_t n);
const void *mulib_uip_pbuf_data(const mulib_uip_pbuf_t *p);
```

## Semantics and safety

Initialize the owner to zero before `init` and destroy it exactly once after use
(destroying a zero owner is harmless). Prepend/append validate the public
capacity/start/length invariants; `data` returns NULL for an inconsistent owner.
Source spans must not overlap the destination being written. Storage is never
grown automatically, and pointers become invalid at destruction.

Packet buffer has fixed allocation after init, max 64 KiB. Prepend headroom must be reserved. Ownership is unique; destroy frees. No reference counting or zero-copy sharing.

## Demonstration

See `../tests/test_uip-pbuf.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-pbuf.h`](../include/mulib-uip-pbuf.h) |
| Implementation switch | `MULIB_IMPL_UIP_PBUF` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-pbuf.c`](../examples/modules/uip-pbuf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-pbuf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
