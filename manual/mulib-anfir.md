# mulib-anfir

**A-normal form** represented by ordered, named let bindings whose operands
are either existing value names or literal atoms. CFG joins use block-parameter
bindings (PHIs); effects are sequenced by binding order.

## Integration and model

Define `MULIB_IMPL_MIR`, `MULIB_IMPL_GSSAIR` and `MULIB_IMPL_ANFIR`, then include
`mulib-anfir.h` in one implementation translation unit. Link `-lm` as needed.
The declarations have C linkage for C++ clients.

Each `mulib_anfir_binding_t.value` is a [GSSA node](mulib-gssair.md). Its ID is
the binding's index. The wrapper adds `literal_mask`, `literals[2]` and
`literal_types[2]`. Bit 0 replaces operand a with a typed literal; bit 1 replaces
b. Only actual scalar operands can be literals. Both literals and named values
are atoms; nested operations are separate bindings. For example:

```c
/* let v1 = add(v0, 42) */
mulib_anfir_binding_t binding = {
  {MULIB_MIR_NATIVE_ADD, MULIB_MIR_I64, 0, 0, MULIB_MIR_NONE,
   0, MULIB_MIR_NONE},
  2, {0, 42}, {MULIB_MIR_VOID, MULIB_MIR_I64}
};
```

Masked operand IDs are ignored. Unmasked scalar operands name bindings.
Nonoperand literal fields are ignored when their mask bits are clear. Masks
outside 0..3 or bits for nonexistent operands are INVALID. PHI/CALL slices and
PARAM's immediate index are metadata, not scalar atoms, and cannot be replaced
by literal bits. Call arguments and incoming PHI values name bindings; create a
CONST binding for an explicit literal there.

`program.graph` supplies blocks, PHI inputs, call arguments and signatures.
Its `nodes` field **must be NULL**, and `node_count` is the binding count.
Terminator values, argument IDs and PHI values all refer to the binding array.
Bindings are grouped by block; PHIs precede each block's ordinary bindings.
All bindings' `memory` fields must be NONE: effects are explicitly sequenced
by their ordered bindings, and normalization constructs memory tokens.

## Transformations and ownership

`mulib_anfir_to_gssair` inserts a CONST immediately before each binding with a
literal operand, assigns new IDs, remaps all references, and builds per-block
memory chains. It then validates types, dominance, joins and CFG reachability.
Forward ordinary references or illegal PHI/control relations fail. Diagnostics
from this final check use expanded node IDs. Literal expansion is not an
optimization and does not remove computations or effects.

`mulib_anfir_from_gssair` validates SSA and derives named-atom bindings with
literal masks zero. Memory tokens are replaced by the already validated binding
order. `mulib_anfir_lower` normalizes and lowers to native mutable-register MIR.
Native arithmetic wraps, as described by GSSA; it does not inherit the older
checked SSA dialect's overflow faults.

Inputs are immutable borrowed descriptors. `mulib_anfir_buffer_t` owns binding
and structural arrays but borrows signature/function metadata. Its `storage`
field is internal backing storage; do not free/edit it independently. GSSA
conversion outputs also borrow metadata. Native MIR output owns its arrays and
copied symbols. Zero-initialize owners, do not copy them, and use
`mulib_anfir_free`; it accepts NULL and resets the owner. Failures preserve
empty outputs. There are no global mutable objects.

## Bounds and example

`limit` bounds input arrays, **expanded** nodes and GSSA's block-squared
analysis. Overflow is checked before allocation. Zero is a literal bound;
NO_MEMORY and RESOURCE_LIMIT are distinct from malformed-reference/type errors.

```sh
cc -std=c11 -Iinclude examples/modules/anfir.c -lm -o anfir
./anfir
```

The example lowers `let v0 = param(0); let v1 = add(v0, 42); return v1`.
`tests/test_microcc.c` exercises literal remapping, invalid masks, GSSA roundtrips
and loop PHIs; `tests/test_microcc_alloc.c` covers failed transformations and
compilation allocations.
