# Conditional probability

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_CONDITIONAL_PROBABLITY`; link `-lm` where required by
your compiler. [Example](../examples/modules/conditional_probablity.c).
The supplied filename spelling is preserved; function names spell probability
correctly.

`mulib_conditional_probability(joint,evidence)` returns P(A and B)/P(B),
requiring 0 <= joint <= evidence <= 1 and evidence > 0.
`mulib_conditional_bayes(likelihood,prior,evidence)` computes the joint as
P(B|A)*P(A). Individual inputs must be finite probabilities and the computed
joint must not exceed evidence. Invalid/undefined results return NAN.
No allocations, ownership, event storage, or independence assumptions are involved.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-conditional_probablity.h`](../include/mulib-conditional_probablity.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-conditional_probablity.h
 * @brief Conditional probability and Bayes' rule for scalar events.
 * Define MULIB_IMPL_CONDITIONAL_PROBABLITY. The historical filename is retained.
 */
#ifndef MULIB_CONDITIONAL_PROBABLITY_H
#define MULIB_CONDITIONAL_PROBABLITY_H
#ifdef __cplusplus
extern "C" {
#endif
/** Return joint/evidence, or NAN unless 0 <= joint <= evidence <= 1 and evidence > 0. */
double mulib_conditional_probability(double joint, double evidence);
/** Return likelihood*prior/evidence; NAN for inconsistent probabilities. */
double mulib_conditional_bayes(double likelihood, double prior, double evidence);
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
| Header | [`include/mulib-conditional_probablity.h`](../include/mulib-conditional_probablity.h) |
| Implementation switch | `MULIB_IMPL_CONDITIONAL_PROBABLITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/conditional_probablity.c`](../examples/modules/conditional_probablity.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/conditional_probablity.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
