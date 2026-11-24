# mulib-cdd-resolve

ISO C11 single-header kernel. To compile this module with its dependencies in exactly one translation unit:

```c
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_PLAN
#define MULIB_IMPL_CDD_RESOLVE
#include "mulib-cdd-resolve.h"
```

Other translation units include the header without switches. See `examples/cdd-pipeline.c` and `tests/cdd-test.c`.

## API
`mulib_cdd_resolve(c,p,max_nodes,out,stats)` minimizes total nonnegative integer cost, then number of entries. All valid activation orders are represented by reachable selected subsets. Memoization is sound because future legal steps depend only on this set (not on order), and cost and count also depend only on the set. A transitive provider closure excludes entries that cannot contribute to any goal or relevant prerequisite. Branch-and-bound prunes states that cannot improve the incumbent. Candidate traversal sorts by cost then ID to make equal-cost outcomes deterministic.

- OK: optimal feasible plan, `found=1`, `stats.optimal=1`.
- UNSAT: no valid plan in the model, exhaustive proof; `found=0`.
- LIMIT: search budget exhausted; `found=1` may contain a validated incumbent, but `stats.optimal=0`. Do not equate LIMIT with UNSAT or optimum.
- INVALID, NOMEM or other validation errors: no plan; `out` and `stats` are cleared if supplied. An oversized catalogue is INVALID here, so LIMIT always denotes exhausted search rather than rejected input. `out` is required.

`max_nodes` must be positive; `stats` is optional and reports expanded states and memo hits. The memo table is capped at 1,048,576 slots and insertion stops at half load. Recursion depth is bounded by 65. Complexity is exponential and budget does not bound every inner operation or wall time. An application may use smaller resolution domains or a higher-level SAT/SMT backend for large problems. Multiple capability providers, conflicting alternatives, external seeds and multi-goal composition are supported.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-cdd-resolve.h`](../kernels/mulib-cdd-resolve.h) |
| Implementation switch | `MULIB_IMPL_CDD_RESOLVE` |
| Repository-local includes | [mulib-cdd-plan](mulib-cdd-plan.md) |
| Complete example | [`examples/cdd-pipeline.c`](../examples/cdd-pipeline.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/cdd-pipeline.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
