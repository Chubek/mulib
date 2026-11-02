# mulib-fst — finite-state transducers

`mulib-fst.h` implements an owned, deterministic Mealy-style finite-state
transducer over `uint32_t` input and output symbols. An edge consumes exactly
one input symbol, matches an inclusive range, and emits either one constant
symbol or nothing. Emitting zero is distinct from emitting nothing.

There are no epsilon edges, weights, nondeterministic branches, final-state
outputs, automatic Unicode decoding, or transducer composition/minimization.
Use [ATN](mulib-atn.md) for epsilon/range nondeterministic recognition.
[Machinebuild](mulib-machinebuild.md) builds these transducers from a DSL and
generates matching JavaScript runtimes. This API is unrelated to compiler
target descriptors in `mulib-machine.h`.

## Integration

Define both switches in exactly one C11 translation unit before inclusion:

```c
#define MULIB_IMPL_VEC
#define MULIB_IMPL_FST
#include "mulib-fst.h"
```

Other translation units include declarations without switches. No mandatory
external dependencies. See [the runnable example](../examples/modules/fst.c):

```sh
cc -std=c11 -Iinclude examples/modules/fst.c -o fst
./fst
```

## Graph ownership and construction

`mulib_fst_create(max_states, max_edges)` returns an empty graph, or NULL on
allocation failure. A zero limit selects 65536 states or 262144 edges,
respectively. `mulib_fst_destroy()` releases it and accepts NULL.

`mulib_fst_add_state(fst, accepting, &id)` assigns a stable, zero-based ID.
Any nonzero accepting flag means accepting. `mulib_fst_set_accepting()` changes
that flag later. There is no implicit start state: callers choose the start on
each run.

`mulib_fst_add_range(fst, from, to, lower, upper, emits, output, &edge_id)`
adds an edge. `edge_id` is optional. Both state IDs must exist and lower must
not exceed upper. Ranges from one state must be disjoint, including endpoints;
overlaps return `MULIB_FST_CONFLICT`, even when the edges otherwise agree.
Any nonzero `emits` emits the specified constant, including `UINT32_MAX` or
zero. A silent edge still consumes one input. Self-loops are allowed.

Mutations preserve the graph and output IDs on failure. State/edge accessors
copy records rather than borrowing internal storage. Counts return zero for
NULL graphs. `first_edge`/`next` traverse outgoing edges in reverse insertion
order, terminating at `MULIB_FST_NONE`. IDs remain stable until destruction.
Concurrent reads are possible only while no thread mutates/destroys the graph;
the module provides no locking.

## Execution

`mulib_fst_step()` tests a token from an explicit current state. On success,
it writes the next state, emits flag, and output symbol. These three outputs
are required and are unchanged on errors or `MULIB_FST_NOMATCH` (missing edge).
For a silent edge, ignore the output symbol. The graph is never mutated.

`mulib_fst_transduce()` consumes the complete input and requires the final
state to be accepting. Missing edges or a nonaccepting final state return
`MULIB_FST_NOMATCH`. Empty input succeeds exactly when the start is accepting.
NULL input is allowed only for length zero.

Initialize `mulib_fst_result_t result = {0}`. A successful transduction replaces
and releases any old result, returning an owned symbol array, its length and
the final state ID. Failure, including NOMATCH, preserves the old result;
partial outputs are never published. `mulib_fst_result_free()` releases and
zeros it, and accepts NULL. Do not copy owning results without transferring
ownership. The result is independent of the graph lifetime.

The optional `work` output is set on every execution return, including errors.
A step charges one unit per token and one per outgoing edge inspected. The
transduce budget covers the entire run, not each token separately. A zero
budget selects 10000000 units. Work never exceeds the budget. Empty runs cost
zero. Exhaustion returns `MULIB_FST_LIMIT` without publishing outputs. Edge
iteration order determines exact work, not matching semantics.

Status values: OK (0), NOMATCH (1), INVALID (-1), NOMEM (-2), LIMIT (-3),
CONFLICT (-4). Invalid pointers/IDs and reversed ranges are INVALID. Capacity
and representational bounds are LIMIT; allocation failure is NOMEM. Callers
must supply valid initialized objects and non-overlapping output parameters
(in particular, `work` must not alias another output).

Storage is O(states + edges); execution scans the outgoing list per token and
uses O(emitted symbols) result storage. Graph construction checks ranges
against existing outgoing edges; bulk insertion can be quadratic.

## Verification

`tests/test_fst_machinebuild.c` covers range boundaries, overlap rejection,
silent/zero/maximal emissions, empty and rejected inputs, exact shared budgets,
transactional results, invalid IDs and injected allocation failures. CMake
discovers it automatically. The machinebuild JavaScript test checks the same
numeric transition semantics in Node when available.
