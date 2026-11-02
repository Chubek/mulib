# PE builder

Define `MULIB_IMPL_PEBUILDER`, `BINFILEWRITER`, `BYCUTILS` and `BUFFER`.
Atomically appends a fixed-base AMD64 PE32+ console image with one .text section.
Entry offset must be inside nonempty code. No imports/runtime/unwind tables/
relocations/signing; freestanding position-independent code only.
[Example](../examples/modules/pebuilder.c), [contracts](storage-server.md).
