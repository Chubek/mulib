# COFF builder

Define `MULIB_IMPL_COFFBUILDER`, `MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_OBJFILEWRITER`,
`MULIB_IMPL_OBJFILEBUILDER`. `mulib_coffbuilder_text` atomically appends a
deterministic AMD64 COFF object with one read/execute .text section aligned to
16 bytes. No symbols/relocations. Input can alias output.
[Example](../examples/modules/coffbuilder.c), [contracts](storage-server.md).

For multiple sections zero-initialize `mulib_coffbuilder_t`, then call `add`,
`finish(builder, native_machine, out)` and `free`. Add copies unique 1..8-byte
names and payloads, with power-of-two alignment <=8192 and shared WRITE/ALLOC/EXEC
flags 1/2/4. Empty payloads may be NULL. Finish supports up to 32767 sections,
requires nonzero machine (AMD64=0x8664), and limits total size to UINT32_MAX.
Names starting with `/` are reserved for COFF long-name references and rejected.
No long-name table, symbols, relocation records, bigobj or archive writer.
Results are 1/0, failures leave output unchanged, finish does not consume builder.
See [binary format contracts](binary-formats.md).
