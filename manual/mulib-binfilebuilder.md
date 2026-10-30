# binfilebuilder

`record` validates a fixed-record schema and appends one zero-padded record to an
initialized buffer. Values are uint64 in schema-field order; count must match,
all fields required and each value must fit its width. Returns 1 success,
0 invalid/range/allocation, without partial output. Schema/values are borrowed
only during call. Result bytes belong to the buffer; no file I/O.

Switches: `MULIB_IMPL_BINFILEBUILDER`, `MULIB_IMPL_BINFILESCHEMA`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/binfilebuilder.c`.
