# mulib-ecad-schematic

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-ecad-schematic.h`  
**Implementation switch:** `MULIB_IMPL_ECAD_SCHEMATIC`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Bounded 256-component flat schematic for R/C/I/V 2-terminal devices, duplicate reference detection and fixed 32-byte names. Graphical wires, multi-sheet hierarchy and bus semantics are future work.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_ECAD_SCHEMATIC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_ECAD_SCHEMATIC
#include "mulib-ecad-schematic.h"
```

## Focused regression test

`tests/test_ecad_schematic.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_ecad_schematic.c -lm -o /tmp/test_ecad_schematic
/tmp/test_ecad_schematic
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ecad-schematic.h`](../include/mulib-ecad-schematic.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ecad-schematic.h
 * @brief Bounded 256-component flat schematic for R/C/I/V 2-terminal devices, duplicate reference
 * detection and fixed 32-byte names.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_SCHEMATIC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_SCHEMATIC_H
#define MULIB_ECAD_SCHEMATIC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ecad_component {
    char ref[32];
    char device;
    char a[32], b[32];
    double value;
} mulib_ecad_component_t;
typedef struct mulib_ecad_schematic {
    mulib_ecad_component_t parts[256];
    size_t count;
} mulib_ecad_schematic_t;
int mulib_ecad_schematic_add(mulib_ecad_schematic_t *s, const mulib_ecad_component_t *c);
int mulib_ecad_schematic_valid(const mulib_ecad_schematic_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_SCHEMATIC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ecad-schematic.h`](../include/mulib-ecad-schematic.h) |
| Implementation switch | `MULIB_IMPL_ECAD_SCHEMATIC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/ecad-schematic.c`](../examples/modules/ecad-schematic.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ecad-schematic.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
