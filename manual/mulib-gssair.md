# mulib-gssair

Typed, gated, memory-aware SSA for one word-valued function. It validates a
control-flow graph and lowers it to the native dialect in [MIR](mulib-mir.md).

## Integration and ownership

Define `MULIB_IMPL_MIR` and `MULIB_IMPL_GSSAIR` in one C11 translation unit,
then include `mulib-gssair.h`; link `-lm` as needed by MIR's other dialects.
All public declarations have C linkage. `mulib_gssair_program_t` borrows immutable
arrays. `mulib_gssair_buffer_t` owns nodes, blocks, PHI inputs and argument IDs,
but **borrows** signature parameter arrays, function descriptors and their names
and signatures. Keep that metadata alive until the buffer is freed.

Zero-initialize owners, never copy live owners, and call `mulib_gssair_free`.
Failure preserves empty outputs. Native MIR returned by `mulib_gssair_lower`
owns its own arrays and symbol strings; it has no source-lifetime dependency.

## Records and semantics

Node IDs are array indices. Nodes are grouped by ascending `block`, with PHIs
first in each block, followed by locally ordered definitions. A block may have
no nodes. The entry is block 0, has no predecessors, and every block must be
reachable. Each block has one separate JUMP, BRANCH or RETURN terminator.

Nodes use `MULIB_MIR_NATIVE_*` value/effect opcodes (not NOP or terminators),
plus `MULIB_GSSAIR_PHI`. Unused `a`, `b`, `memory` are `MULIB_MIR_NONE`; unused
immediates are zero. Scalar operations refer to node IDs in a/b. CONST carries
literal bits in `immediate`; PARAM carries its signature argument index there.
CALL uses `immediate` as function index and `a,b` as start/count in `arguments`.
Every call returns a value; STORE is the sole VOID node.

| Type | Meaning |
|---|---|
| I64 | 64-bit two's-complement integer bits; arithmetic wraps |
| WORD | unsigned word / native address bits |
| BOOL | canonical 0 or 1 |
| VOID | STORE only; cannot be used as a value |

Arithmetic and bit operations require equal I64 or WORD operands/result. COPY
preserves type. EQ accepts equal non-VOID types and produces BOOL. SLT and ULT
compare equal I64/WORD operands with explicitly signed/unsigned interpretation.
LOAD takes WORD and returns I64 or WORD; STORE takes a WORD address and a scalar
value. LOAD/STORE always transfer **eight bytes**, including a stored BOOL. To
obtain a BOOL from loaded data, compare it. CALL and PARAM must match their
signatures. Function signature results are I64/WORD/BOOL, not VOID/F64.
BOOL parameters and external call results must obey the declared canonical ABI.

JUMP uses `yes`; BRANCH uses `value` (BOOL), `yes` and `no`; RETURN uses `value`
matching the signature result. Other fields are NONE. Branch successors can be
equal; predecessor sets count distinct blocks, not parallel CFG edges.

## Gates, dominance and memory

A PHI's `a,b` select `inputs`, each `{predecessor,value}`. There must be exactly
one input for every CFG predecessor, without duplicates. The taken incoming
edge gates which value is bound. An incoming definition must dominate that
predecessor's terminator. This permits backedge references and loop-carried
self/cyclic PHIs. Ordinary uses must be dominated by their definitions; local
uses refer to an earlier node. PHIs cannot appear at entry or after block body
operations. Unreachable blocks and edges into the external entry are rejected.

Every LOAD, STORE and CALL names the preceding such node in its **own block**
through `memory`. The first uses NONE. All other nodes use NONE. This is one
conservative, may-alias memory chain; block-entry memory is implicitly gated by
the incoming control edge. There is no claim of disjoint alias regions, memory
speculation or atomic accesses. Faulting loads/stores and calls remain ordered
through native lowering and scheduling.

## APIs and bounds

`mulib_gssair_shape` checks array sizes, reference ranges, node types, signature
arity/types, symbols and terminator shapes without ordering/dominance analysis.
This supports unordered graph transformations. It does not make an arbitrary
graph executable. `mulib_gssair_validate` additionally checks reachability,
dominance, PHI predecessor sets, ordered definitions and exact memory chains.
Dense block analysis needs `block_count * block_count <= limit` bytes/cells.
All descriptor arrays and aggregate external-function parameter count also fit
`limit`. Zero is a real bound; multiplication and generated-code sizes are
checked. Diagnosed indices refer to the relevant node or descriptor array.

`mulib_gssair_permute(program, order, limit, out, diagnostic)` copies a
shape-valid graph with `order[new_id] = old_id` (NULL means identity). It remaps
all scalar references, memory tokens, arguments, terminator values and PHI
values. It checks that order is a permutation but does not assert SSA order;
validate the result or pass it to ClickIR. Block IDs are unchanged.

`mulib_gssair_lower` validates, then assigns a virtual register to each node.
Every edge entering a block with PHIs gets a split block: first copy all
incoming values into fresh temporaries, then copy temporaries into PHI result
registers and jump to the destination. This preserves **parallel** assignment,
including loop swaps and critical edges. Unused VOID-node slots are harmless.
CALL arguments become native register-ID slices and symbols are copied.

## Example and verification

```sh
cc -std=c11 -Iinclude examples/modules/gssair.c -lm -o gssair
./gssair
```

The example lowers a constant-return function. `tests/test_microcc.c` also
checks loop-carried parallel copies, critical edges, ordering, dominance,
duplicate/missing PHI inputs, unreachable blocks, types, memory chains and
limits. Allocation-failure coverage is in `tests/test_microcc_alloc.c`.
