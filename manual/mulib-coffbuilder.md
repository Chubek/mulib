# COFF builder

Define `MULIB_IMPL_COFFBUILDER`, `BYCUTILS` and `BUFFER`. Atomically appends a
deterministic AMD64 COFF object with one read/execute .text section aligned to
16 bytes. No symbols/relocations. Input can alias output.
[Example](../examples/modules/coffbuilder.c), [contracts](storage-server.md).
