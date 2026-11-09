# mulib-pcb-pnp

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-pcb-pnp.h`  
**Implementation switch:** `MULIB_IMPL_PCB_PNP`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

CSV pick-and-place placement export with millimetre XY, degrees and top/bottom side; no footprint origins/centroid correction, assembly variant or machine-specific profile.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_PCB_PNP` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_PCB_PNP
#include "mulib-pcb-pnp.h"
```

## Focused regression test

`tests/test_pcb_pnp.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_pcb_pnp.c -lm -o /tmp/test_pcb_pnp
/tmp/test_pcb_pnp
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-pcb-pnp.h`](../include/mulib-pcb-pnp.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-pcb-pnp.h
 * @brief CSV pick-and-place placement export with millimetre XY, degrees and top/bottom side; no
 * footprint origins/centroid correction, assembly variant or machine-specific profile.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_PNP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_PNP_H
#define MULIB_PCB_PNP_H
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_placement {
    char reference[48];
    double x, y, rotation_deg;
    uint8_t side;
} mulib_pcb_placement_t;
int mulib_pcb_pnp_write(FILE *fp, const mulib_pcb_placement_t *parts, size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_PNP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pcb-pnp.h`](../include/mulib-pcb-pnp.h) |
| Implementation switch | `MULIB_IMPL_PCB_PNP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/pcb-pnp.c`](../examples/modules/pcb-pnp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pcb-pnp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
