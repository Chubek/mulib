# mulib-superblocks

Single-entry traces over typed SSA, with explicit side exits and native MIR
lowering. It chooses block layout without moving effects across branches.

## Integration

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_GSSAIR` and `MULIB_IMPL_SUPERBLOCKS`, then
include `mulib-superblocks.h` in one C11 translation unit. Link `-lm` if needed
by MIR. Declarations have C linkage.

`mulib_superblocks_program_t` contains a [GSSA graph](mulib-gssair.md), a complete
block permutation `order`, and `starts[trace_count+1]` partitioning that
permutation. Each trace is nonempty. `starts[0] == 0` and the final sentinel is
`block_count`. The first block remains entry block 0. Only a trace's head can
have predecessors outside that trace. Every subsequent block has exactly its
immediately preceding trace block as its distinct CFG predecessor. Branches may
leave the trace through side exits; those edges remain explicit.

## Building and validating

`mulib_superblocks_build` validates SSA, then starts at block 0 and extends a
trace while a successor is unvisited and has a single predecessor. It prefers
the true successor, then the false successor. When a trace ends, it chooses the
lowest unvisited block as the next head. This yields deterministic maximal
traces under that policy, not a profile-guided optimal layout. Backedges and
join blocks remain valid CFG edges. There is no tail duplication or speculation.

`mulib_superblocks_validate` checks the graph, permutation, boundaries,
connectivity and single-entry property. A custom trace layout can be supplied
using the borrowed descriptor; malformed permutations or side entrances fail
with BAD_REFERENCE.

`mulib_superblocks_to_gssair` lays out blocks in trace order and remaps every
block ID, node ID, terminator, memory chain, call argument and incoming PHI gate.
It validates the resulting ordered graph. `mulib_superblocks_lower` then applies
GSSA's edge-split parallel-copy lowering. PHI edge blocks are appended after the
trace-ordered ordinary blocks. Low-level scheduling remains block-local, so
operations are not executed on a path that originally bypassed them.

## Ownership and limits

Descriptors borrow all arrays. `mulib_superblocks_buffer_t` owns a copy of the
graph's structural arrays, its order and starts arrays. It borrows signature
and function metadata, as GSSA does. Zero-initialize it and free with
`mulib_superblocks_free`, which resets it and accepts NULL. Do not copy a live
owner or free its nested graph separately. All output construction is atomic.
Native MIR output independently owns arrays and symbol names.

`limit` bounds graph arrays and GSSA's block-squared analysis. Order has exactly
`block_count` entries; starts reserves `block_count+1` entries for its sentinel.
Allocation multiplication/addition is checked. Zero limits remain literal.
Array bounds and valid backing objects remain the C caller's responsibility.

## Example and checks

```sh
cc -std=c11 -Iinclude examples/modules/superblocks.c -lm -o superblocks
./superblocks
```

The example forms two traces from a branch with two returns. MicroCC tests
exercise loop/PHI trace derivation, semantic equivalence, assembly execution and
allocation failures.
