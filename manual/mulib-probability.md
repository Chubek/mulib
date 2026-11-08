# Discrete probability

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_PROBABILITY` in one C11 translation unit; link `-lm`.
See [example](../examples/modules/probability.c).

`mulib_probability_normalize` divides nonnegative finite weights by their
sum using maximum scaling to avoid sum overflow. It requires nonempty,
positive mass; failure returns 0 without mutation. All arrays are borrowed.
Validation requires each entry in [0,1] and a sum within the supplied finite,
nonnegative absolute tolerance. Empty distributions are invalid.

Entropy uses natural logarithms, with 0 log 0 = 0, and validates to 1e-9.
Invalid entropy inputs return NAN. `logsumexp` accepts negative/positive
infinity, returns -INFINITY for empty/all-negative-infinity input, and NAN for
NaN or a missing nonempty array. All operations are linear, allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-probability.h`](../include/mulib-probability.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-probability.h
 * @brief Discrete probability validation, normalization, entropy and log sums.
 * Define MULIB_IMPL_PROBABILITY in one C translation unit. Link libm.
 */
#ifndef MULIB_PROBABILITY_H
#define MULIB_PROBABILITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Validate nonnegative finite entries summing to one within tolerance. */
int mulib_probability_valid(const double *p, size_t n, double tolerance);
/** Normalize finite nonnegative weights in place; failure leaves them unchanged. */
int mulib_probability_normalize(double *weights, size_t n);
/** Shannon entropy in nats; NAN for an invalid distribution. */
double mulib_probability_entropy(const double *p, size_t n);
/** Log(sum(exp(x))); empty input gives -INFINITY, invalid/NaN gives NAN. */
double mulib_probability_logsumexp(const double *x, size_t n);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-probability.h`](../include/mulib-probability.h) |
| Implementation switch | `MULIB_IMPL_PROBABILITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/probability.c`](../examples/modules/probability.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/probability.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
