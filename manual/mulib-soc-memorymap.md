# mulib-soc-memorymap

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-soc-memorymap.h`  
**Implementation switch:** `MULIB_IMPL_SOC_MEMORYMAP`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Bounded 128-entry nonoverlapping physical address map with overflow-checked half-open regions, device IDs and offset lookup. No actual bus arbitration or memory-mapped peripheral execution.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SOC_MEMORYMAP` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SOC_MEMORYMAP
#include "mulib-soc-memorymap.h"
```

## Focused regression test

`tests/test_soc_memorymap.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_soc_memorymap.c -lm -o /tmp/test_soc_memorymap
/tmp/test_soc_memorymap
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-soc-memorymap.h`](../include/mulib-soc-memorymap.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-soc-memorymap.h
 * @brief Bounded 128-entry nonoverlapping physical address map with overflow-checked half-open
 * regions, device IDs and offset lookup.
 * @details C11 single-header implementation; define MULIB_IMPL_SOC_MEMORYMAP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SOC_MEMORYMAP_H
#define MULIB_SOC_MEMORYMAP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_soc_range {
    uint64_t start, length;
    uint32_t device;
} mulib_soc_range_t;
typedef struct mulib_soc_memorymap {
    mulib_soc_range_t ranges[128];
    size_t count;
} mulib_soc_memorymap_t;
int mulib_soc_memorymap_add(mulib_soc_memorymap_t *map, uint64_t start, uint64_t length,
                            uint32_t device);
int mulib_soc_memorymap_lookup(const mulib_soc_memorymap_t *map, uint64_t address, uint32_t *device,
                               uint64_t *offset);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SOC_MEMORYMAP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-soc-memorymap.h`](../include/mulib-soc-memorymap.h) |
| Implementation switch | `MULIB_IMPL_SOC_MEMORYMAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/soc-memorymap.c`](../examples/modules/soc-memorymap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/soc-memorymap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
