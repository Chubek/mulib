# mulib-hwil-types

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-hwil-types.h`  
**Implementation switch:** `MULIB_IMPL_HWIL_TYPES`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Validation and exact matching of bit-vector and single-bit clock types. MEMORY enum is reserved and deliberately rejected until memory shape semantics exist.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_HWIL_TYPES` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_HWIL_TYPES
#include "mulib-hwil-types.h"
```

## Focused regression test

`tests/test_hwil_types.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_hwil_types.c -lm -o /tmp/test_hwil_types
/tmp/test_hwil_types
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-hwil-types.h`](../include/mulib-hwil-types.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-hwil-types.h
 * @brief Validation and exact matching of bit-vector and single-bit clock types.
 * @details C11 single-header implementation; define MULIB_IMPL_HWIL_TYPES in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HWIL_TYPES_H
#define MULIB_HWIL_TYPES_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_hwil_kind {
    MULIB_HWIL_BITS = 1,
    MULIB_HWIL_CLOCK = 2,
    MULIB_HWIL_MEMORY = 3
} mulib_hwil_kind_t;
typedef struct mulib_hwil_type {
    mulib_hwil_kind_t kind;
    uint32_t width;
    uint8_t signedness, fourstate;
} mulib_hwil_type_t;
int mulib_hwil_type_valid(mulib_hwil_type_t t);
int mulib_hwil_type_equal(mulib_hwil_type_t a, mulib_hwil_type_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HWIL_TYPES_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hwil-types.h`](../include/mulib-hwil-types.h) |
| Implementation switch | `MULIB_IMPL_HWIL_TYPES` |
| Repository-local includes | None |
| Complete example | [`examples/modules/hwil-types.c`](../examples/modules/hwil-types.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hwil-types.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
