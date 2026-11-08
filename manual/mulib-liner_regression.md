# Simple linear regression

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_LINER_REGRESSION`; link `-lm`.
[Example](../examples/modules/liner_regression.c). Filename spelling retained.

Fits y=intercept+slope*x by ordinary least squares using scaled centered sums.
The result is a copied plain struct with slope, intercept and r_squared.
Input arrays are borrowed. Need at least two finite paired observations and
nonconstant x; invalid pointers, undefined fit or coefficients outside double
range return 0 without output mutation. Constant y yields r_squared=1.
Predict evaluates using fma; NULL model/nonfinite x gives NAN, overflow may
give INFINITY. No allocation, O(n) fit time. Near-constant data remains limited
by floating-point precision. No multiple predictors, regularization, confidence
intervals or statistical significance tests are implied.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-liner_regression.h`](../include/mulib-liner_regression.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-liner_regression.h
 * @brief Ordinary least-squares regression of one predictor with an intercept.
 * Define MULIB_IMPL_LINER_REGRESSION; link libm. Filename retained.
 */
#ifndef MULIB_LINER_REGRESSION_H
#define MULIB_LINER_REGRESSION_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_linear_regression { double slope, intercept, r_squared; } mulib_linear_regression_t;
/** Fit y=intercept+slope*x using scaled centered sums. Return 0 for n<2,
 * nonfinite data, constant x, unrepresentable coefficients or invalid pointers.
 * Output is unchanged on failure. Constant y gives r_squared=1. */
int mulib_linear_regression_fit(const double *x, const double *y, size_t n, mulib_linear_regression_t *result);
/** Evaluate fitted line; NAN for NULL model or nonfinite x. */
double mulib_linear_regression_predict(const mulib_linear_regression_t *model, double x);
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
| Header | [`include/mulib-liner_regression.h`](../include/mulib-liner_regression.h) |
| Implementation switch | `MULIB_IMPL_LINER_REGRESSION` |
| Repository-local includes | None |
| Complete example | [`examples/modules/liner_regression.c`](../examples/modules/liner_regression.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/liner_regression.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
