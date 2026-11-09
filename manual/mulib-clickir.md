# mulib-clickir

[Manuals](README.md) · [Library index](index.md)

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
The complete effect chain explicitly orders may-alias LOAD/STORE/CALL nodes and
the trapping SDIV/SREM integer operations.

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

`mulib_clickir_validate` checks the same unordered graph without retaining an
owner. `mulib_clickir_clone` validates and copies both structural arrays and
additional precedence edges, preserving the input's physical node order. It does
not silently drop precedence constraints or accept a partially live destination.
Self edges and cross-block memory predecessors are explicitly rejected.

## Register import and executable lowering

`mulib_clickir_from_native` imports a typed mutable native-MIR register program.
Supply one fixed I64/BOOL/WORD type per register plus a borrowed signature. The
converter adds a synthetic entry with typed zero initial register values, then
PHIs for every register in every original block. It remaps uses/definitions and
fills incoming values from predecessor exits, preserving loops and cyclic parallel
copies. Every original block must be reachable. Native NOPs disappear. Memory and
external calls are unsupported by this importer; SDIV/SREM are explicitly ordered
as trapping effects. Frontends requiring definite assignment must verify it before
import, rather than treating the native zero initialization as bytecode validity.

`mulib_clickir_to_flow` normalizes and lowers an I64/BOOL graph into an owned,
safe MIR register image. WORD, memory and calls return UNSUPPORTED. Wrapping
arithmetic, native six-bit masked shifts (including SAR), signed/unsigned compares
and PHI parallel copies retain their semantics. Signed divide/remainder trap on
zero and INT64_MIN/-1; they are not Java's width-specific MIN/-1 operations.
The Android/JVM frontends implement 32-bit Java semantics with explicit widening
and truncation around these operations.

The flow owner owns instructions/types and borrows signature parameters. Release
it with `mulib_clickir_flow_free`. Its immutable program can initialize MIR frames,
feed the existing native JIT, or emit portable C AOT. No external compiler or
unrestricted host-memory interpreter is introduced into ClickIR.

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
Register import also bounds register*block cells, generated nodes/PHI inputs and
the enlarged synthetic-entry CFG. Flow lowering conservatively reserves up to
five records per native instruction and three integer scratch registers, all
within the supplied limit. These explicit, dense algorithms target small graphs.

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

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-clickir.h`](../include/mulib-clickir.h) |
| Implementation switch | `MULIB_IMPL_CLICKIR` |
| Repository-local includes | [mulib-gssair](mulib-gssair.md) |
| Complete example | [`examples/modules/clickir.c`](../examples/modules/clickir.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/clickir.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
