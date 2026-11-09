# mulib-ecad-erc

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-ecad-erc.h`  
**Implementation switch:** `MULIB_IMPL_ECAD_ERC`  
**Dependencies:** `ecad-schematic`.

## Purpose and supported profile

Flat schematic ERC diagnostics: self-shorted components, single-use (dangling) named terminals, and missing ground node. No powered pin type matrix, output-output contention or hidden power pins.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_ECAD_ERC` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_ECAD_ERC
#include "mulib-ecad-erc.h"
```

## Focused regression test

`tests/test_ecad_erc.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_ecad_erc.c -lm -o /tmp/test_ecad_erc
/tmp/test_ecad_erc
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ecad-erc.h`](../include/mulib-ecad-erc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ecad-erc.h
 * @brief Flat schematic ERC diagnostics: self-shorted components, single-use (dangling) named
 * terminals, and missing ground node.
 * @details C11 single-header implementation; define MULIB_IMPL_ECAD_ERC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ECAD_ERC_H
#define MULIB_ECAD_ERC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-ecad-schematic.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ecad_erc_result {
    size_t isolated, duplicate_shorts, unconnected_ground;
} mulib_ecad_erc_result_t;
int mulib_ecad_erc_run(const mulib_ecad_schematic_t *s, mulib_ecad_erc_result_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ECAD_ERC_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ecad-erc.h`](../include/mulib-ecad-erc.h) |
| Implementation switch | `MULIB_IMPL_ECAD_ERC` |
| Repository-local includes | [mulib-ecad-schematic](mulib-ecad-schematic.md) |
| Complete example | [`examples/modules/ecad-erc.c`](../examples/modules/ecad-erc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ecad-erc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
