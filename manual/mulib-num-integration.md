# mulib-num-integration

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-num-integration.h`  
**Implementation switch:** `MULIB_IMPL_NUM_INTEGRATION`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Composite trapezoid and Simpson quadrature, plus adaptive trapezoids using a Richardson error estimate and at most 30 refinement rounds. Callbacks must be finite over the interval. The estimate is not a rigorous error bound and can miss unresolved oscillations or singularities.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_NUM_INTEGRATION` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_NUM_INTEGRATION
#include "mulib-num-integration.h"
```

## Focused regression test

`tests/test_num_integration.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_num_integration.c -lm -o /tmp/test_num_integration
/tmp/test_num_integration
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-num-integration.h`](../include/mulib-num-integration.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-num-integration.h
 * @brief Composite trapezoid and Simpson quadrature, plus adaptive trapezoids using a Richardson
 * error estimate and at most 30 refinement rounds.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_INTEGRATION in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_INTEGRATION_H
#define MULIB_NUM_INTEGRATION_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_num_integrand)(double t, void *user);
int mulib_num_integrate_trapezoid(mulib_num_integrand f, void *user, double lo, double hi,
                                  size_t intervals, double *out);
int mulib_num_integrate_simpson(mulib_num_integrand f, void *user, double lo, double hi,
                                size_t intervals, double *out);
/* Composite trapezoids, doubling panels until the Richardson error estimate
 * is within absolute tolerance. Returns failure when the refinement budget ends. */
int mulib_num_integrate_adaptive(mulib_num_integrand f, void *user, double lo, double hi,
                                 double tolerance, size_t refinements, double *out, double *error);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_INTEGRATION_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-num-integration.h`](../include/mulib-num-integration.h) |
| Implementation switch | `MULIB_IMPL_NUM_INTEGRATION` |
| Repository-local includes | None |
| Complete example | [`examples/modules/num-integration.c`](../examples/modules/num-integration.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/num-integration.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
