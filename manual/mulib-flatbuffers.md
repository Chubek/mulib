# flatbuffers

Borrowed checked table views, not a FlatBuffers implementation/compiler.
`root` opens a non-size-prefixed LE32 root offset; `table` validates the signed
vtable distance, vtable/object lengths and bounds (including shared forward
vtables), rejecting overlapping vtable/object ranges. `uint` reads a present field of width 1..8; `string` follows a forward
offset, checking length and NUL termination. Returns 1 valid/found, 0 malformed,
absent or invalid; outputs unchanged on failure. Input must remain immutable.
Field access also checks the view's full ranges before adding offsets. String
offsets must skip their complete four-byte offset field.

No schema/default interpretation, vectors/unions/nested table accessors, writer,
UTF-8 checking or whole-buffer verification. Caller chooses correct scalar width.
Do not treat local field checks as validation of every reachable object.
Switches: `MULIB_IMPL_FLATBUFFERS`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BINFILE`.
Example: `examples/modules/flatbuffers.c`.
