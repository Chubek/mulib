# mulib-ecad-symbol

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-ecad-symbol.h`  
**Implementation switch:** `MULIB_IMPL_ECAD_SYMBOL`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Bounded symbol metadata with 32 typed pins and integer drawing-space anchor positions. No SVG geometry, rotations, or font rendering.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_ECAD_SYMBOL` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_ECAD_SYMBOL
#include "mulib-ecad-symbol.h"
```

## Focused regression test

`tests/test_ecad_symbol.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_ecad_symbol.c -lm -o /tmp/test_ecad_symbol
/tmp/test_ecad_symbol
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ecad-symbol.h`](../include/mulib-ecad-symbol.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ecad-symbol.h
 * @brief Bounded symbol metadata with 32 typed pins and integer drawing-space anchor positions.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_SYMBOL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_SYMBOL_H
#define MULIB_ECAD_SYMBOL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_ecad_pin_kind {
    MULIB_ECAD_PASSIVE = 0,
    MULIB_ECAD_INPUT = 1,
    MULIB_ECAD_OUTPUT = 2,
    MULIB_ECAD_POWER_IN = 3,
    MULIB_ECAD_POWER_OUT = 4
} mulib_ecad_pin_kind_t;
typedef struct mulib_ecad_pin {
    char name[32];
    int x, y;
    mulib_ecad_pin_kind_t kind;
} mulib_ecad_pin_t;
typedef struct mulib_ecad_symbol {
    char name[48];
    size_t pins;
    mulib_ecad_pin_t pin[32];
} mulib_ecad_symbol_t;
int mulib_ecad_symbol_pin(mulib_ecad_symbol_t *s, const char *name, int x, int y,
                          mulib_ecad_pin_kind_t kind);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_SYMBOL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ecad-symbol.h`](../include/mulib-ecad-symbol.h) |
| Implementation switch | `MULIB_IMPL_ECAD_SYMBOL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ecad-symbol.c`](../examples/modules/ecad-symbol.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ecad-symbol.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
