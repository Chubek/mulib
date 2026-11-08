# Bayesian updating

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_BAYESIAN_METHODS` and `MULIB_IMPL_PROBABILITY`; link `-lm`.
[Example](../examples/modules/bayesian_methods.c).

`mulib_bayesian_update` normalizes log(prior)+log_likelihood with log-sum-exp.
Prior must be a nonempty distribution (tolerance 1e-9), log likelihoods <= 0,
including -INFINITY for impossible evidence. This API is for discrete event
likelihoods, not probability densities greater than one. Zero total evidence,
invalid inputs, size overflow and allocation failure return 0.

Output is caller-owned with n slots and may alias prior, but not likelihoods.
Optional log_evidence must be separate from arrays. Failure preserves outputs.
Temporary storage is O(n), time O(n). This is finite-hypothesis updating, not
a general inference/sampling framework.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-bayesian_methods.h`](../include/mulib-bayesian_methods.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-bayesian_methods.h
 * @brief Stable finite-hypothesis Bayesian updating using log likelihoods.
 * Define MULIB_IMPL_BAYESIAN_METHODS and MULIB_IMPL_PROBABILITY. Link libm.
 */
#ifndef MULIB_BAYESIAN_METHODS_H
#define MULIB_BAYESIAN_METHODS_H
#include "mulib-probability.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Update a normalized prior; log likelihoods must be <= 0 (including -INFINITY).
 * Output may alias prior, not likelihoods. Failure leaves output unchanged.
 * Zero evidence fails. Optional log_evidence receives the normalization constant.
 */
int mulib_bayesian_update(const double *prior, const double *log_likelihood,
                         size_t n, double *posterior, double *log_evidence);
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
| Header | [`include/mulib-bayesian_methods.h`](../include/mulib-bayesian_methods.h) |
| Implementation switch | `MULIB_IMPL_BAYESIAN_METHODS` |
| Repository-local includes | [mulib-probability](mulib-probability.md) |
| Complete example | [`examples/modules/bayesian_methods.c`](../examples/modules/bayesian_methods.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bayesian_methods.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
