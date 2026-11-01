# mulib-clickir

Block-pinned **Sea of Nodes** with explicit data, control placement and memory
dependencies. Node order is arbitrary; normalization creates ordered GSSA and
lowering creates native MIR.

## Integration

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_GSSAIR` and `MULIB_IMPL_CLICKIR` before
including `mulib-clickir.h` in one translation unit. Link `-lm` where MIR
requires it. Declarations support C++ clients through C linkage.

`mulib_clickir_program_t.graph` uses the node, block, PHI-input, signature and
call-argument records described in [GSSA](mulib-gssair.md). Nodes may be in any
array order and refer forward locally. A node's `block` fixes where it executes;
there is no implicit placement inference. PHI inputs gate control-flow merges.
The complete memory chain explicitly orders may-alias LOAD/STORE/CALL nodes.

`edges[edge_count]` adds `{before,after}` precedence constraints within one
block. Endpoints must be valid node IDs in the same block. These constrain
normalization order; later backend scheduling can reorder independent pure
operations, while all memory effects remain ordered. Cross-block placement is
expressed through the CFG and PHIs, not these additional local edges.

## Normalization

`mulib_clickir_to_gssair` checks graph shape and additional edges, then
topologically orders each block using local data uses, call arguments, memory
dependencies and extra precedence edges. Ready PHIs precede ordinary nodes;
otherwise ties use the lowest node ID. Cross-block incoming PHI values are not
local topological dependencies, so loop-carried cycles remain legal. A local
data/memory/precedence cycle returns `MULIB_MIR_BAD_REFERENCE`.

The transformation remaps every value ID, including PHIs, terminators, memory
and call arguments, then validates dominance, predecessor gates and memory
chains. It rejects disconnected memory chains, effects pinned to inconsistent
blocks, non-dominating values and illegal node types rather than dropping them.
Errors from the final check use **normalized** node IDs.

`mulib_clickir_lower` performs normalization followed by GSSA's parallel-copy
native lowering. `mulib_clickir_from_gssair` validates and copies an already
ordered graph into an owned Sea of Nodes with no additional edges. Clients can
use `mulib_gssair_permute` to construct differently ordered graph structures.

## Ownership, bounds and failures

Program descriptors and input arrays are borrowed and immutable. An owned
`mulib_clickir_buffer_t` owns structural arrays and extra edges, but borrows
signature/function metadata. A normalized GSSA buffer has the same metadata
lifetime rule. Native MIR output owns its symbol strings and all arrays.
Zero-initialize output owners; failed transformations leave them unchanged.
`mulib_clickir_free` is NULL-safe and resets the owner.

`limit` bounds each array, additional edges, and the block-squared analysis
storage used by GSSA validation. Zero is not unlimited. Allocation-size overflow
and allocation failure have distinct RESOURCE_LIMIT/NO_MEMORY results. Normalization
uses repeated ready-node scans; it is a bounded-storage implementation rather
than a linear-time graph scheduler. All C descriptors must name readable arrays.

## Example

`examples/modules/clickir.c` supplies ADD before its two constant operands,
normalizes it and lowers the result:

```sh
cc -std=c11 -Iinclude examples/modules/clickir.c -lm -o clickir
./clickir
```

`tests/test_microcc.c` reverses a cyclic-loop SSA graph's physical node order,
compiles it through ClickIR, checks semantics, and rejects local precedence
cycles and out-of-range edges.
