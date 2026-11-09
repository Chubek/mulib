# mulib-pcb-bom

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-pcb-bom.h`  
**Implementation switch:** `MULIB_IMPL_PCB_BOM`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Deterministic fixed-size BOM aggregator by manufacturer and exact part number, with checked size_t quantity accumulation. No inventory, sourcing, DNP variants or CSV export.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_PCB_BOM` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_PCB_BOM
#include "mulib-pcb-bom.h"
```

## Focused regression test

`tests/test_pcb_bom.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_pcb_bom.c -lm -o /tmp/test_pcb_bom
/tmp/test_pcb_bom
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pcb-bom.h`](../include/mulib-pcb-bom.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pcb-bom.h
 * @brief Deterministic fixed-size BOM aggregator by manufacturer and exact part number, with
 * checked size_t quantity accumulation.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_BOM in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_BOM_H
#define MULIB_PCB_BOM_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_bom_line {
    char mfr[48], part_number[48];
    size_t quantity;
} mulib_pcb_bom_line_t;
typedef struct mulib_pcb_bom {
    mulib_pcb_bom_line_t lines[256];
    size_t count;
} mulib_pcb_bom_t;
int mulib_pcb_bom_add(mulib_pcb_bom_t *b, const char *manufacturer, const char *part_number,
                      size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_BOM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pcb-bom.h`](../include/mulib-pcb-bom.h) |
| Implementation switch | `MULIB_IMPL_PCB_BOM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/pcb-bom.c`](../examples/modules/pcb-bom.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pcb-bom.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
