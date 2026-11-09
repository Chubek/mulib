# mulib-uip-fragment

**One-datagram IPv4 payload reassembly with overlap rejection and bounded memory**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_UIP_FRAGMENT` once before including `mulib-uip-fragment.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_uip_frag {uint8_t bytes[8192],occupied[8192];uint16_t id;uint8_t src[4],dst[4],protocol;size_t final_size;int initialized,has_last;} mulib_uip_frag_t;
void mulib_uip_frag_reset(mulib_uip_frag_t *r);
int mulib_uip_frag_push(mulib_uip_frag_t *r,const uint8_t src[4],const uint8_t dst[4],uint8_t protocol,
                        uint16_t id,size_t offset,int more,const void *data,size_t n,size_t *complete_size);
```

## Semantics and safety

Fragments must be nonempty. A last fragment cannot shorten the datagram below
bytes already received. Conflicting final lengths and overlaps fail without
changing the recorded final size. Call `mulib_uip_frag_reset` before starting
another datagram and to discard an incomplete/expired datagram; the caller owns
timeout policy. `complete_size` is zero until all bytes are present.

One reassembly context, max 8192-byte IPv4 payload, no timeouts and no header reassembly. Reject overlapping fragments; return 1 on complete payload, 0 incomplete, negative errno on rejection. Caller must reset on timeout/key change. Requires input caller to validate IPv4 fragment consistency and header checksums.

## Demonstration

See `../tests/test_uip-fragment.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-uip-fragment.h`](../include/mulib-uip-fragment.h) |
| Implementation switch | `MULIB_IMPL_UIP_FRAGMENT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/uip-fragment.c`](../examples/modules/uip-fragment.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/uip-fragment.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
