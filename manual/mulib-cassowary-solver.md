# mulib-cassowary-solver

[Manuals](README.md) · [Library index](index.md)

A Cassowary-style linear constraint tableau solver: required equalities and
inequalities, nonnegative slack/error variables, artificial-variable feasibility
and simplex minimization of weighted absolute errors. It is separate from the
nonlinear least-squares `mulib-geosolve` module.

Define `MULIB_IMPL_CASSOWARY_SOLVER` in one C11 translation unit; link libm.
There are no MuLIB or third-party dependencies.

## API and constraint semantics

`create(NULL)` owns an initially empty solver. `add_variable` returns a stable
zero-based variable ID initialized to zero. Variables are unrestricted in sign;
add explicit bounds when required. `add(terms, count, relation, rhs, strength,
&constraint_id)` expresses `sum(coefficient * variable) relation rhs` where
relation is EQ, LE or GE. Terms are copied; repeated variables are summed.
Input coefficients, RHS and strengths must be finite.

- `MULIB_CASSOWARY_REQUIRED` is **0** and makes a constraint hard.
- Positive strengths minimize weighted L1 violation. Named WEAK/MEDIUM/STRONG
  values are 1/1000/1000000, as numeric weights, not exact lexicographic levels.
- Equality violation is absolute residual; LE/GE charge only the violated side.

`add_edit(variable, positive_strength)` installs a soft equality at the current
value. `suggest` changes its target and immediately resolves. There is at most
one edit constraint per variable. `remove_edit`/`remove` remove constraints and
resolve. Edit targets can be infeasible relative to hard bounds: the solver
chooses the best feasible value rather than weakening required constraints.
Explicit soft stay constraints can preserve otherwise free coordinates.

All failed mutations preserve the logical constraints, edit targets and
published variable values; required contradictions return UNSATISFIABLE.
Constraint IDs are stable and not reused after successful removal; failed adds
do not consume an ID. The configured constraint cap counts allocated ID slots,
including removed constraints. `constraint_count` reports active constraints.
`value` borrows no state: it copies one published result into an output double.
`destroy` accepts NULL and frees term copies and all solver storage.

## Algorithm and limits

Each change rebuilds a fresh tableau from retained constraints, substituting
basic variables, introducing dummy/slack/error variables, and using an
artificial objective when no direct subject exists. Primal simplex optimizes
the real weighted-error objective. This implementation deliberately trades
rebuild cost for transactional rollback; it does **not** claim Kiwi-compatible
incremental dual-simplex edit performance. `solve` explicitly rebuilds too.

Zero options select 128 variables, 512 constraint ID slots, 2048 symbols,
10000000 tableau work units and absolute epsilon 1e-8. Configurable caps are
1024 variables, 4096 constraints, 8192 symbols. Dense rows mean memory/time grow
with both symbols and constraints; inappropriate large problems can fail
allocation or hit work limits. Symbols include errors, slacks and artificial
variables, not just user variables. No native recursive algorithm is used.

The solver uses double arithmetic and a configured zero tolerance; this is not
exact rational arithmetic. It reports NUMERIC for unrepresentable operations
or failed post-solve hard-constraint residual checks. Scale ill-conditioned
constraints sensibly. Degenerate optima can have multiple valid answers;
nonbasic external variables default to zero. This is a continuous linear solver,
not an integer, nonlinear, quadratic or strict-inequality solver.

Status is zero success or negative INVALID/MEMORY/UNSATISFIABLE/LIMIT/NUMERIC.
Required residuals are independently checked before publication, with relative
scaling based on the sum of absolute terms and RHS. Priority weights can be
outvoted by sufficiently many lower-weight constraints; use required constraints
for invariants, not arbitrarily huge floating-point weights.

See [`examples/modules/cassowary-solver.c`](../examples/modules/cassowary-solver.c).
`tools/check_cassowary.py` compares 100 deterministic 2D problems against an
independent exact-rational vertex enumeration of the weighted-L1 objective;
unit tests also cover edits, redundant constraints, infeasibility and rollback.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cassowary-solver.h`](../include/mulib-cassowary-solver.h) |
| Implementation switch | `MULIB_IMPL_CASSOWARY_SOLVER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/cassowary-solver.c`](../examples/modules/cassowary-solver.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cassowary-solver.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
