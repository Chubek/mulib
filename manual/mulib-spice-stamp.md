# mulib-spice-stamp

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-spice-stamp.h`  
**Implementation switch:** `MULIB_IMPL_SPICE_STAMP`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

MNA matrix and RHS allocation and linear conductance/independent current/ideal voltage-source stamps. Nodes: ground=0, unknown nodes indexed 1..N. Voltage-source branch index is zero-based MNA unknown index. Nodes must not alias branch positions as physical node IDs.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_SPICE_STAMP` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_SPICE_STAMP
#include "mulib-spice-stamp.h"
```

## Focused regression test

`tests/test_spice_stamp.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_spice_stamp.c -lm -o /tmp/test_spice_stamp
/tmp/test_spice_stamp
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-spice-stamp.h`](../include/mulib-spice-stamp.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-spice-stamp.h
 * @brief MNA matrix and RHS allocation and linear conductance/independent current/ideal
 * voltage-source stamps.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_STAMP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_STAMP_H
#define MULIB_SPICE_STAMP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_spice_stamp {
    size_t unknowns;
    double *a, *b;
} mulib_spice_stamp_t;
int mulib_spice_stamp_init(mulib_spice_stamp_t *s, size_t unknowns);
void mulib_spice_stamp_free(mulib_spice_stamp_t *s);
int mulib_spice_stamp_conductance(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double g);
int mulib_spice_stamp_current(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double amps);
int mulib_spice_stamp_voltage(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, size_t branch,
                              double volts);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_STAMP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-spice-stamp.h`](../include/mulib-spice-stamp.h) |
| Implementation switch | `MULIB_IMPL_SPICE_STAMP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/spice-stamp.c`](../examples/modules/spice-stamp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/spice-stamp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
