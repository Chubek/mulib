# MuLIB smt

[Manuals](README.md) · [Library index](index.md) · [Pricing integration](mulib-pricing-suite.md)

A bounded SMT solver for Boolean CNF and floating-point real difference constraints.

## Purpose and contract

The supported arithmetic fragment is `x_i - x_j <= c`, with finite double-valued `c` (evaluated using long-double Bellman–Ford relaxations). Each difference atom is optionally guarded by one signed SAT literal or unconditionally enabled. This is a narrow SMT difference-logic engine, not a general SMT-LIB implementation.

## Algorithms and numerical semantics

**Algorithm.** An internal MuLIB SAT solver finds a Boolean model. The theory checker collects all active difference constraints and runs Bellman–Ford on an implicit super-source (all distances initialized to zero). If it detects a negative cycle, the current complete Boolean assignment is blocked by a new CNF clause, and SAT is solved again. Since each blocked assignment is theory-infeasible independently of additional constraints, persistent blocking is logically sound. This enumerates Boolean assignments and can be exponential; `max_models` (default 10,000) is the bound. A `LIMIT` status means that no final satisfiability conclusion was reached.

**Variables.** Boolean variables `1..bools`, reals `0..reals-1`; both are bounded at 4096. Guard `0` makes a difference constraint always active; guard `+b` requires Boolean b true, `-b` requires it false. `mulib_smt_clause` and `mulib_smt_difference` return Boolean success and invalidate the prior model. `solve` returns `MULIB_SMT_SAT`, `UNSAT`, `INVALID`, `LIMIT`, or `MEMORY`. Only after SAT can Boolean and real values be queried. Real-valued answers may be negative and are one feasible witness, not a unique solution.

**Precision.** Theory arithmetic uses `long double`, then publishes finite double-valued witnesses. Since decimal input bounds are converted to binary floating point, this is not an exact rational arithmetic solver; numerical boundary cases deserve external verification. There is no integer arithmetic, arbitrary linear programming, multiplication of variables, optimization, arrays, strings, bitvectors, quantifiers, or parser for SMT-LIB. Constraint `x>=L` can be expressed as `zero-x<=-L` using a designated reference variable fixed by relative values; all difference systems are translation-invariant. To encode an absolute upper bound, use `x-zero<=U` and treat `zero` as the origin when reading witness differences.

Rounded double witnesses are rechecked against active constraints using
long-double differences. Nonfinite or constraint-violating rounded witnesses
return `LIMIT`, not `MEMORY` or `UNSAT`, expose no model, and do not block the
Boolean assignment. Floating-point relaxation is not a proof procedure for
arbitrary real inputs: extremely ill-conditioned constraints may lose small
increments even in `long double`. Use an exact-arithmetic solver when certified
SAT/UNSAT conclusions are required. Failed constraint insertions preserve the
previous model; successful insertions and every solve invalidate it.

**Complexity.** Exponential in Boolean variable count in the worst case; each theory check takes up to O(real_variables×difference_constraints) time and O(real_variables) space. The SAT solver also enforces a decision budget.

## Example

```c
#define MULIB_IMPL_SAT
#define MULIB_IMPL_SMT
#include "mulib-smt.h"
#include <assert.h>
int main(void) {
    mulib_smt_t *s = mulib_smt_create(1, 2, 100);
    double x, y;
    assert(s && mulib_smt_difference(s, 0, 1, -3, 1));
    assert(mulib_smt_clause(s, (int[]){1}, 1));
    assert(mulib_smt_solve(s) == MULIB_SMT_SAT);
    assert(mulib_smt_real_value(s, 0, &x));
    assert(mulib_smt_real_value(s, 1, &y));
    assert(x - y <= -3);
    mulib_smt_destroy(s);
    return 0;
}
```

Compile from repository root (using the additions copied into its directories):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude example.c -lm -o example
```

The `-lm` link flag is for the system C math library, not an external third-party dependency.

## Integration and limitations

Implementation macro: `MULIB_IMPL_SMT`. Declare it in **one** translation unit before including the module. Required additional MuLIB implementation switches: `sat`. Define all such switches before their headers are first included. The implementation must not be emitted in more than one translation unit. The code is ISO C11 and exposes C linkage for C++17 clients. For the full test and multi-translation-unit recipe, see [Pricing integration](mulib-pricing-suite.md).

This SMT solver is an optional peripheral module: pricing methods do not require Boolean or SMT solving. Prefer numerical methods for elasticity and forecast fitting.

All algorithms are deterministic. Data collections are borrowed; their lifetimes must cover the function call. Return-code and output-mutation guarantees are documented above and in the public API declarations. None of these modules accesses networks or file systems.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-smt.h`](../include/mulib-smt.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-smt.h
 * @brief Bounded SMT for Boolean CNF + floating-point real difference logic
 * (x_i - x_j <= c) via SAT enumeration and Bellman-Ford theory checking.
 * This is NOT general-purpose SMT-LIB, bitvectors, linear arithmetic or an
 * industrial DPLL(T) solver. Define MULIB_IMPL_SAT and MULIB_IMPL_SMT;
 * include this header in one C11 TU. No external dependencies.
 */
#ifndef MULIB_SMT_H
#define MULIB_SMT_H
#include <stddef.h>
#include "mulib-sat.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_smt mulib_smt_t;
/** SAT status semantics; LIMIT also covers unrepresentable theory witnesses. */
typedef enum mulib_smt_status {
  MULIB_SMT_SAT=1, MULIB_SMT_UNSAT=0, MULIB_SMT_INVALID=-1,
  MULIB_SMT_LIMIT=-2, MULIB_SMT_MEMORY=-3
} mulib_smt_status_t;
/** Boolean variables 1..bools, real variables 0..reals-1. */
mulib_smt_t *mulib_smt_create(size_t bools, size_t reals, size_t max_models);
void mulib_smt_destroy(mulib_smt_t *solver);
/** CNF clause, signed Boolean literals. Returns 1 on success. */
int mulib_smt_clause(mulib_smt_t *solver, const int *literals, size_t n);
/** Add x[left] - x[right] <= bound, active iff guard literal holds.
 * guard=0 means always active, otherwise signed Boolean literal. */
int mulib_smt_difference(mulib_smt_t *solver, size_t left, size_t right,
    double bound, int guard);
/** Enumerate up to max_models Boolean assignments, using long-double
 * Bellman-Ford relaxation (not exact rational arithmetic).
 * LIMIT also reports numerical failure; no model is exposed in that case. */
int mulib_smt_solve(mulib_smt_t *solver);
int mulib_smt_boolean_value(const mulib_smt_t *solver, size_t variable);
/** Returns 1 and writes real value only for a current SAT model. */
int mulib_smt_real_value(const mulib_smt_t *solver, size_t variable, double *out);
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
| Header | [`include/mulib-smt.h`](../include/mulib-smt.h) |
| Implementation switch | `MULIB_IMPL_SMT` |
| Repository-local includes | [mulib-sat](mulib-sat.md) |
| Complete example | [`examples/modules/smt.c`](../examples/modules/smt.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/smt.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
