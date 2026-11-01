# mulib-depir

An entity/dependency representation from which executable Sea of Nodes,
A-normal form, gated SSA, superblocks and native MIR can be derived.

## Integration

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_GSSAIR`, `MULIB_IMPL_CLICKIR`,
`MULIB_IMPL_ANFIR`, `MULIB_IMPL_SUPERBLOCKS` and `MULIB_IMPL_DEPIR`, then include
`mulib-depir.h` in one implementation translation unit. Link `-lm` where needed.
Declarations have C linkage for C++ callers.

## Entities and relations

`mulib_depir_program_t.graph` contains executable records using the GSSA schema,
but may have unordered nodes. The separate entity table names:

| Kind | `entity.index` |
|---|---|
| VALUE | node ID, including VOID effect nodes |
| BLOCK | control-flow block ID |
| FUNCTION | external function/signature index |
| TYPE | `MULIB_MIR_I64`, `BOOL` or `WORD` |
| DATA | a CONST node carrying literal data |

`edges` reference **entity indices**, not node indices. Relations are
DATA_FLOW, CONTROL, MEMORY, TYPING and CALL. They describe dependencies between
values, definitions, blocks, functions and types. Metadata relations may be
cyclic; recursive-function metadata and loop control flow are not errors merely
because the entire dependency graph is not a DAG. This module represents one
function's executable body plus its external function/type dependencies, not a
whole source-language type system, function-body module or data-section linker.

`mulib_depir_from_gssair` validates and copies a graph and constructs entities
for all nodes, blocks, external functions, three scalar types and constants.
It derives operand and call-argument dependencies, incoming PHI values and gates,
memory chains, block membership, CFG edges, terminator value dependencies,
callee references and function/value type relationships. The constant's DATA
entity points to its literal-producing VALUE entity.

The executable graph retains operation payloads and signatures; dependency
edges alone do not encode arithmetic literals or invent executable operations.
Additional local VALUE->VALUE DATA_FLOW/CONTROL/MEMORY edges constrain node
ordering. Other relations describe metadata or inter-block control/data
dependence. They do not implicitly execute, duplicate, or remove graph nodes.

## Derivation APIs

- `mulib_depir_validate`: check graph **shape**, entity kinds/indices, relation
  tags and endpoints. It does not assert local acyclicity or dominance.
- `mulib_depir_to_clickir`: copy the executable graph and retain local
  VALUE->VALUE ordering edges. Incoming PHI data relationships are gates,
  not intra-block scheduling edges, so those are excluded from the local DAG.
- `mulib_depir_to_gssair`: order that Sea of Nodes and validate SSA, memory
  chains and CFG gates.
- `mulib_depir_to_anfir`: derive named let bindings from the ordered SSA.
- `mulib_depir_to_superblocks`: derive validated single-entry trace layout.
- `mulib_depir_lower`: derive ordered SSA and lower to native mutable-register
  MIR, including edge-local parallel PHI copies.

Executable operands always contribute dependencies even if the metadata table
omits their edges. Contradictory local ordering creates a cycle and fails during
normalization. Cross-block relationships remain represented by the executable
CFG, typed operands and PHI gates. Derived forms retain execution semantics;
they are not lossless serialization roundtrips for extra analysis metadata.

## Ownership, bounds and errors

Input descriptors are borrowed and immutable. `mulib_depir_buffer_t` owns its
entity/edge arrays and a structural graph copy, but borrows function/signature
metadata. Derived high-level buffers also own structure and borrow that metadata;
native output owns symbols and needs no source lifetime. Initialize all owners
to zero, never copy live owners, and use the matching free function. Free accepts
NULL; failed construction leaves empty outputs unchanged.

`limit` bounds each entity, edge and IR array, aggregate function parameters and
block-squared SSA analysis. Automatically derived entity and edge arrays must
also fit it; it is not just an input node limit. Count additions and allocation
multiplication are checked. Cycles/indices, types, resource limits and allocation
failure have distinct MIR diagnostics; indices refer to the failing stage's
entity/edge/node array.

## Example

```sh
cc -std=c11 -Iinclude examples/modules/depir.c -lm -o depir
./depir
```

The example derives dependency records, ANF and native MIR from a scalar return.
MicroCC tests derive every supported representation from loop-bearing DepIR,
execute the generated assembly, and inject failure at every allocation.
