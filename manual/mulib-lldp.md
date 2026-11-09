# mulib-lldp

**Decode bounds-checked Ethernet LLDP TLVs**

Status: implemented for the explicitly documented subset; Linux-specific unless noted. This add-on has no third-party dependencies. It does not replace a complete userspace network stack or network manager.

## Build and integration

Define `MULIB_IMPL_LLDP` once before including `mulib-lldp.h` in one C11 translation unit. Include normally in other translation units. Dependent implementation switches are **not enabled automatically**.

Direct new-module dependencies: (none).

## Public API

```c
typedef struct mulib_lldp_tlv {uint8_t type;const uint8_t *value;size_t length;} mulib_lldp_tlv_t;
int mulib_lldp_foreach(const void *buf,size_t n,int (*cb)(const mulib_lldp_tlv_t*,void*),void *ctx);
```

## Semantics and safety

TLV decoder only. Stops at zero-length end TLV; requires complete LLDPDU. MAC/Ethernet filtering done by caller.

## Demonstration

See `../tests/test_lldp.c` for its compile/link verification. Selected runnable demos are under `../examples/`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lldp.h`](../include/mulib-lldp.h) |
| Implementation switch | `MULIB_IMPL_LLDP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/lldp.c`](../examples/modules/lldp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lldp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
