# binfileschema

Borrowed fixed-record schema: array of named fields with byte offsets and widths
1..8. `validate` requires nonzero record size, unique nonempty names, bounded
fields and no overlaps. Gaps are permitted. No arrays, nesting, strings, native
padding assumptions or variable-length data model.

`get` validates schema and decodes a named unsigned LE field from a complete
record. Returns 1 success, 0 invalid/missing/truncated; value unchanged on error.
Schema/field names must remain live and immutable while used.
Switches: `MULIB_IMPL_BINFILESCHEMA`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/binfileschema.c`.
