# mulib-spice-independent

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-spice-independent.h`  
**Implementation switch:** `MULIB_IMPL_SPICE_INDEPENDENT`  
**Dependencies:** `spice-stamp`.

## Purpose and supported profile

Time-independent DC current and voltage MNA source stamps. Source waveforms and AC phasors are not implemented in this module.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SPICE_INDEPENDENT` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SPICE_INDEPENDENT
#include "mulib-spice-independent.h"
```

## Focused regression test

`tests/test_spice_independent.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_spice_independent.c -lm -o /tmp/test_spice_independent
/tmp/test_spice_independent
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-spice-independent.h`](../include/mulib-spice-independent.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-spice-independent.h
 * @brief Time-independent DC current and voltage MNA source stamps.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_INDEPENDENT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_INDEPENDENT_H
#define MULIB_SPICE_INDEPENDENT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-stamp.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_independent_current(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double value);
int mulib_spice_independent_voltage(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, size_t branch,
                                    double value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_INDEPENDENT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-spice-independent.h`](../include/mulib-spice-independent.h) |
| Implementation switch | `MULIB_IMPL_SPICE_INDEPENDENT` |
| Repository-local includes | [mulib-spice-stamp](mulib-spice-stamp.md) |
| Complete example | [`examples/modules/spice-independent.c`](../examples/modules/spice-independent.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spice-independent.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
