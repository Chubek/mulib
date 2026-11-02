# ELF builder

Define `MULIB_IMPL_ELFBUILDER`, `OBJFILEBUILDER`, `OBJFILEWRITER`, `BINFILEWRITER`,
`BYCUTILS` and `BUFFER`. Reuses owned section accumulation and atomic ELF64
little-endian ET_REL writing. Names/data are copied; flags/alignment follow
objfilebuilder. No symbols/relocations/executable segments.
[Example](../examples/modules/elfbuilder.c), [contracts](storage-server.md).
