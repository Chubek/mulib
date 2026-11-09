# mulib-rf-microstrip

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-rf-microstrip.h`  
**Implementation switch:** `MULIB_IMPL_RF_MICROSTRIP`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Quasi-static zero-thickness microstrip impedance estimate for width/substrate thickness ratio and dielectric constant. Approximate closed-form model only: no conductor thickness, dispersion, roughness or full-wave analysis.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_RF_MICROSTRIP` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_RF_MICROSTRIP
#include "mulib-rf-microstrip.h"
```

## Focused regression test

`tests/test_rf_microstrip.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_rf_microstrip.c -lm -o /tmp/test_rf_microstrip
/tmp/test_rf_microstrip
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rf-microstrip.h`](../include/mulib-rf-microstrip.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-rf-microstrip.h
 * @brief Quasi-static zero-thickness microstrip impedance estimate for width/substrate thickness
 * ratio and dielectric constant.
 * @details C11 single-header implementation; define MULIB_IMPL_RF_MICROSTRIP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_RF_MICROSTRIP_H
#define MULIB_RF_MICROSTRIP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_microstrip_z0(double width_mm, double substrate_mm, double epsilon_r,
                           double *impedance_ohm);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RF_MICROSTRIP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rf-microstrip.h`](../include/mulib-rf-microstrip.h) |
| Implementation switch | `MULIB_IMPL_RF_MICROSTRIP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/rf-microstrip.c`](../examples/modules/rf-microstrip.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rf-microstrip.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
