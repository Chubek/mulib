# MuLIB sat

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

A small complete exact propositional CNF solver for MuLIB, with explicit search budgets.

## Purpose and contract

Standalone DPLL solver with unit propagation and deterministic variable order. Only the standard C runtime is used. It is intentionally not CDCL, does not implement conflict learning, clause deletion, watched literals, MaxSAT, or optimization.

## Algorithms and numerical semantics

**Semantics.** Variables are numbered `1..variables` and literals are positive/negative variable IDs; literal zero is invalid. Clauses are ORs; zero-length clause means `false`. Formula is the AND of clauses; zero clauses means `true`. `mulib_sat_solve(solver, assumptions, n)` adds assumptions **temporarily** for that solve only; it does not alter stored clauses. Models report `+1` or `-1`, and `0` only when unavailable or invalid. Unconstrained model variables are assigned a valid arbitrary Boolean value (here `false`).

**Status codes.** `1` SAT, `0` UNSAT, `-1` invalid assumption/pointer, `-2` search decision limit, `-3` memory error. `add_clause` returns Boolean success, not these statuses. `create` returns NULL for invalid capacities or allocation failure. The default decision limit is one million, and variable capacity is bounded at 4096. The solver searches both values for every necessary decision variable and is complete if it returns SAT or UNSAT; LIMIT is not UNSAT.

**Complexity.** DPLL is exponential in the worst case and unit propagation currently rescans all clauses until a fixed point. It is appropriate for integration tests, modest finite configuration problems, and prototyping. Clause arrays are owned by the solver; arguments are copied. A clause insertion or solve invalidates the previous model; failures to insert do not mutate the solver.

**Use with pricing.** One can encode Boolean product flags and pricing-rule eligibility. This is not required by any of the current numeric pricing engines and should not be used for floating-point nonlinear optimization.

## Example

```c
#define MULIB_IMPL_SAT
#include "mulib-sat.h"
#include <assert.h>
int main(void) {
    mulib_sat_t *sat = mulib_sat_create(2, 0);
    const int clause1[] = {1, 2};
    const int clause2[] = {-1, 2};
    assert(sat && mulib_sat_add_clause(sat, clause1, 2));
    assert(mulib_sat_add_clause(sat, clause2, 2));
    assert(mulib_sat_solve(sat, NULL, 0) == MULIB_SAT_SAT);
    assert(mulib_sat_value(sat, 2) == +1);
    mulib_sat_destroy(sat);
    return 0;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_SAT`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: None. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

The optional SMT difference-logic module builds on this solver. SAT is purely propositional and has no arithmetic knowledge.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sat.h`](../include/mulib-sat.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-sat.h
 * @brief Bounded exact CNF SAT using DPLL, deterministic branching, unit
 * propagation, assumptions and model retrieval. Not a CDCL solver.
 * Define MULIB_IMPL_SAT in one C11 translation unit. No external libraries.
 */
#ifndef MULIB_SAT_H
#define MULIB_SAT_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sat mulib_sat_t;
/** SAT=1, UNSAT=0, invalid=-1, search limit=-2, OOM=-3. */
typedef enum mulib_sat_status {
  MULIB_SAT_SAT=1, MULIB_SAT_UNSAT=0, MULIB_SAT_INVALID=-1,
  MULIB_SAT_LIMIT=-2, MULIB_SAT_MEMORY=-3
} mulib_sat_status_t;
/** Variables numbered 1..variables; variables<=4096; 0 permitted.
 * max_decisions=0 selects 1000000 recursive decision nodes. */
mulib_sat_t *mulib_sat_create(size_t variables, size_t max_decisions);
void mulib_sat_destroy(mulib_sat_t *solver);
/** Add a clause of signed literals +/-[1,variables]. Empty clause is false;
 * no modification on invalid input/OOM. Returns 1 on success. */
int mulib_sat_add_clause(mulib_sat_t *solver, const int *literals, size_t n);
/** Solve with optional temporary unit assumptions. A SAT model remains valid
 * until next mutation/solve. Returns mulib_sat_status_t. */
int mulib_sat_solve(mulib_sat_t *solver, const int *assumptions, size_t n);
/** Get model value (+1/-1). Returns 0 if no SAT model or invalid variable. */
int mulib_sat_value(const mulib_sat_t *solver, size_t variable);
size_t mulib_sat_variables(const mulib_sat_t *solver);
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
| Header | [`include/mulib-sat.h`](../include/mulib-sat.h) |
| Implementation switch | `MULIB_IMPL_SAT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/sat.c`](../examples/modules/sat.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sat.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
