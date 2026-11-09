# mulib-eda-units

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-units.h`  
**Implementation switch:** `MULIB_IMPL_EDA_UNITS`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

SI base dimensions and a deliberately scoped engineering unit set (m, kg, s, A, K, mol, cd, Hz, V, Ohm, F, H, W, C, degC, rad, deg). Supports T/G/M/k/m/u/n/p/f SI prefixes (case-sensitive) and Celsius temperature offset. Not a complete dimensional expression parser.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_UNITS` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_UNITS
#include "mulib-eda-units.h"
```

## Focused regression test

`tests/test_eda_units.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_units.c -lm -o /tmp/test_eda_units
/tmp/test_eda_units
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-units.h`](../include/mulib-eda-units.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-units.h
 * @brief SI base dimensions and a deliberately scoped engineering unit set (m, kg, s, A, K, mol,
 * cd, Hz, V, Ohm, F, H, W, C, degC, rad, deg).
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_UNITS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_UNITS_H
#define MULIB_EDA_UNITS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_dimension {
    int8_t exp[7];
} mulib_eda_dimension_t;
typedef struct mulib_eda_unit {
    const char *symbol;
    mulib_eda_dimension_t dim;
    double scale, offset;
} mulib_eda_unit_t;
int mulib_eda_unit_lookup(const char *name, mulib_eda_unit_t *out);
int mulib_eda_parse_engineering(const char *text, double *out, mulib_eda_dimension_t *dim);
int mulib_eda_convert(double value, const char *from, const char *to, double *out);
int mulib_eda_dimension_equal(mulib_eda_dimension_t a, mulib_eda_dimension_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_UNITS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-units.h`](../include/mulib-eda-units.h) |
| Implementation switch | `MULIB_IMPL_EDA_UNITS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-units.c`](../examples/modules/eda-units.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-units.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
