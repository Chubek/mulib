# mulib-rf-smith

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-rf-smith.h`  
**Implementation switch:** `MULIB_IMPL_RF_SMITH`  
**Dependencies:** `rf-sparams`.

## Purpose and supported profile

Smith chart Cartesian coordinates from normalized single-port impedance using the S11 transform. Does not draw curves or support admittance chart overlays.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_RF_SMITH` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_RF_SMITH
#include "mulib-rf-smith.h"
```

## Focused regression test

`tests/test_rf_smith.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_rf_smith.c -lm -o /tmp/test_rf_smith
/tmp/test_rf_smith
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rf-smith.h`](../include/mulib-rf-smith.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-rf-smith.h
 * @brief Smith chart Cartesian coordinates from normalized single-port impedance using the S11
 * transform.
 * @details C11 single-header implementation; define MULIB_IMPL_RF_SMITH in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_RF_SMITH_H
#define MULIB_RF_SMITH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-rf-sparams.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_smith_xy(mulib_rf_complex_t impedance, double z0, double *x, double *y);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RF_SMITH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-rf-smith.h`](../include/mulib-rf-smith.h) |
| Implementation switch | `MULIB_IMPL_RF_SMITH` |
| Repository-local includes | [mulib-rf-sparams](mulib-rf-sparams.md) |
| Complete example | [`examples/modules/rf-smith.c`](../examples/modules/rf-smith.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rf-smith.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
