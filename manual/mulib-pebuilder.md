# PE builder

Define `MULIB_IMPL_PEBUILDER`, `MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_BYCUTILS`,
`MULIB_IMPL_BUFFER`, `MULIB_IMPL_OBJFILEWRITER`, `MULIB_IMPL_OBJFILEBUILDER`.
Atomically appends a fixed-base AMD64 PE32+ console image with one .text section.
Entry offset must be inside nonempty code. No imports/runtime/unwind tables/
relocations/signing; freestanding position-independent code only.
[Example](../examples/modules/pebuilder.c), [contracts](storage-server.md).

The owned multi-section API uses zero-initialized `mulib_pebuilder_t` and
`add`/`finish`/`free`. Add copies unique 1..8-byte names and payloads; alignment
is a power of two <=4096, flags are shared WRITE/ALLOC/EXEC bits 1/2/4. Names
starting with `/` are rejected. `finish(builder, 0x8664, out)` writes an AMD64
PE32+ **data-only DLL profile with no entry point**, not a runnable application.
Use `text` for a freestanding executable. Up to 96 sections, 512-byte file
alignment and 4096-byte section alignment; file/image offsets must fit 32 bits.
Every section is raw initialized data (including any caller-supplied zeroes).
All writes append atomically, returning 1/0. Finish does not consume builder.
See [ownership and format contracts](binary-formats.md).
