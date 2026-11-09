# mulib-rf-sparams

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-rf-sparams.h`  
**Implementation switch:** `MULIB_IMPL_RF_SPARAMS`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Single-port complex impedance/S11 conversion with scaled arithmetic and VSWR for reflection magnitude below one. Real positive reference impedance is required. No multiport matrices or Touchstone codec.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_RF_SPARAMS` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_RF_SPARAMS
#include "mulib-rf-sparams.h"
```

## Focused regression test

`tests/test_rf_sparams.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_rf_sparams.c -lm -o /tmp/test_rf_sparams
/tmp/test_rf_sparams
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rf-sparams.h`](../include/mulib-rf-sparams.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-rf-sparams.h
 * @brief Single-port complex impedance/S11 conversion with scaled arithmetic and VSWR for
 * reflection magnitude below one.
 * @details C11 single-header implementation; define MULIB_IMPL_RF_SPARAMS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_RF_SPARAMS_H
#define MULIB_RF_SPARAMS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_rf_complex {
    double re, im;
} mulib_rf_complex_t;
int mulib_rf_s11_from_impedance(mulib_rf_complex_t impedance, double reference_ohm,
                                mulib_rf_complex_t *reflection);
int mulib_rf_impedance_from_s11(mulib_rf_complex_t reflection, double reference_ohm,
                                mulib_rf_complex_t *impedance);
int mulib_rf_s11_vswr(mulib_rf_complex_t reflection, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RF_SPARAMS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rf-sparams.h`](../include/mulib-rf-sparams.h) |
| Implementation switch | `MULIB_IMPL_RF_SPARAMS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rf-sparams.c`](../examples/modules/rf-sparams.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rf-sparams.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
