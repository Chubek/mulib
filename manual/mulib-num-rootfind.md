# mulib-num-rootfind

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-num-rootfind.h`  
**Implementation switch:** `MULIB_IMPL_NUM_ROOTFIND`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Bracketed scalar bisection and damped Newton iteration with bounded residual-decreasing backtracking. Explicit iteration budgets and finite-value checks; failed computations preserve output. Bisection uses an overflow-safe midpoint.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_NUM_ROOTFIND` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_NUM_ROOTFIND
#include "mulib-num-rootfind.h"
```

## Focused regression test

`tests/test_num_rootfind.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_num_rootfind.c -lm -o /tmp/test_num_rootfind
/tmp/test_num_rootfind
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-num-rootfind.h`](../include/mulib-num-rootfind.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-num-rootfind.h
 * @brief Bracketed scalar bisection and damped Newton iteration with bounded residual-decreasing
 * backtracking.
 * @details C11 single-header implementation; define MULIB_IMPL_NUM_ROOTFIND in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_NUM_ROOTFIND_H
#define MULIB_NUM_ROOTFIND_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef double (*mulib_num_scalar_fn)(double x, void *user);
int mulib_num_bisect(mulib_num_scalar_fn f, void *user, double lo, double hi, double tol,
                     size_t max_iter, double *out);
int mulib_num_newton(mulib_num_scalar_fn f, mulib_num_scalar_fn deriv, void *user, double x,
                     double tol, size_t steps, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_NUM_ROOTFIND_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-num-rootfind.h`](../include/mulib-num-rootfind.h) |
| Implementation switch | `MULIB_IMPL_NUM_ROOTFIND` |
| Repository-local includes | None |
| Complete example | [`examples/modules/num-rootfind.c`](../examples/modules/num-rootfind.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/num-rootfind.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
