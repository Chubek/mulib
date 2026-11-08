# Kernel mean matching (KMM)

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_KMM`; link `-lm`. [Example](../examples/modules/kmm.c).
KMM here means **kernel mean matching**, not k-means clustering.

Solves min 0.5 wᵀKw - κᵀw subject to 0<=w<=bound and sum(w)=n, the
epsilon=0 variant of KMM. Caller supplies n*n symmetric positive-semidefinite
kernel entries and κ[i]=(n/m) sum_j kernel(source_i,target_j). Kernel
construction is external. Exact symmetry and finite entries are checked;
positive semidefiniteness/conditioning are caller preconditions, not verified.
Require n>0, bound>=1, positive finite tolerance and positive iteration budget.

Projected gradient uses a row-sum Lipschitz bound and 128 bisection steps for
box/simplex projection. Return 1 for successive max coordinate change <=
tolerance, 2 on iteration exhaustion (approximate weights), 0 invalid/OOM/
numerical failure. Stopping is not an optimality certificate; equality is met
only to floating-point accuracy. Inputs are borrowed, output has n slots and
must not overlap inputs. Failure leaves output unchanged. O(n) temporary
storage, O(iterations*(n²+128n)) time. No relaxed-sum constraint or kernel loader.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-kmm.h`](../include/mulib-kmm.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-kmm.h
 * @brief Kernel mean matching by projected gradient on a capped simplex.
 * Define MULIB_IMPL_KMM; link libm. KMM means kernel mean matching, not k-means.
 */
#ifndef MULIB_KMM_H
#define MULIB_KMM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Minimize 0.5*w'K*w-kappa'w, with 0<=w<=bound and sum(w)=n (epsilon=0).
 * K is borrowed row-major n*n, symmetric positive semidefinite (PSD is a caller
 * precondition); kappa[i]=(n/m)*sum_j kernel(source_i,target_j).
 * bound>=1, tolerance>0, iterations>0. Output has n slots, does not overlap inputs.
 * Return 1 if successive max change <= tolerance, 2 on iteration limit (valid
 * approximate weights), 0 invalid/OOM/numerical failure (output unchanged).
 * Convergence is a numerical stopping test, not an optimality certificate.
 */
int mulib_kmm_fit(const double *kernel, const double *kappa, size_t n, double bound,
                  size_t iterations, double tolerance, double *weights);
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
| Header | [`include/mulib-kmm.h`](../include/mulib-kmm.h) |
| Implementation switch | `MULIB_IMPL_KMM` |
| Repository-local includes | None |
| Complete example | [`examples/modules/kmm.c`](../examples/modules/kmm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/kmm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
