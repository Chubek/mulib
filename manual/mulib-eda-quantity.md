# mulib-eda-quantity

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-quantity.h`  
**Implementation switch:** `MULIB_IMPL_EDA_QUANTITY`  
**Dependencies:** `eda-units`.

## Purpose and supported profile

First-order uncertainty propagation for independent unit-tagged quantities with addition, multiplication and division. Dimension overflow, nonfinite values, negative uncertainties and division by zero are rejected. Correlated errors and affine temperature arithmetic are outside this profile.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_QUANTITY` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_QUANTITY
#include "mulib-eda-quantity.h"
```

## Focused regression test

`tests/test_eda_quantity.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_quantity.c -lm -o /tmp/test_eda_quantity
/tmp/test_eda_quantity
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-quantity.h`](../include/mulib-eda-quantity.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-quantity.h
 * @brief First-order uncertainty propagation for independent unit-tagged quantities with addition,
 * multiplication and division.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_QUANTITY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_QUANTITY_H
#define MULIB_EDA_QUANTITY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-units.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_quantity {
    double value, sigma;
    mulib_eda_dimension_t dim;
} mulib_eda_quantity_t;
int mulib_eda_quantity_add(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out);
int mulib_eda_quantity_mul(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out);
int mulib_eda_quantity_div(mulib_eda_quantity_t a, mulib_eda_quantity_t b,
                           mulib_eda_quantity_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_QUANTITY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-quantity.h`](../include/mulib-eda-quantity.h) |
| Implementation switch | `MULIB_IMPL_EDA_QUANTITY` |
| Repository-local includes | [mulib-eda-units](mulib-eda-units.md) |
| Complete example | [`examples/modules/eda-quantity.c`](../examples/modules/eda-quantity.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-quantity.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
