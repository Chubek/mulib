# mulib-field-poisson

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-field-poisson.h`  
**Implementation switch:** `MULIB_IMPL_FIELD_POISSON`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Closed-form homogeneous 1D electrostatic potential and electric field with uniform free charge and fixed boundary potentials. No multidimensional mesh or capacitance extraction.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_FIELD_POISSON` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_FIELD_POISSON
#include "mulib-field-poisson.h"
```

## Focused regression test

`tests/test_field_poisson.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_field_poisson.c -lm -o /tmp/test_field_poisson
/tmp/test_field_poisson
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-field-poisson.h`](../include/mulib-field-poisson.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-field-poisson.h
 * @brief Closed-form homogeneous 1D electrostatic potential and electric field with uniform free
 * charge and fixed boundary potentials.
 * @details C11 single-header implementation; define MULIB_IMPL_FIELD_POISSON in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FIELD_POISSON_H
#define MULIB_FIELD_POISSON_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_field_poisson_1d(double length_m, double permittivity_f_m, double charge_c_m3,
                           double phi_left, double phi_right, double x_m, double *potential_volts);
int mulib_field_poisson_field_1d(double length, double permittivity, double charge, double left,
                                 double right, double x, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FIELD_POISSON_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-field-poisson.h`](../include/mulib-field-poisson.h) |
| Implementation switch | `MULIB_IMPL_FIELD_POISSON` |
| Repository-local includes | None |
| Complete example | [`examples/modules/field-poisson.c`](../examples/modules/field-poisson.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/field-poisson.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
