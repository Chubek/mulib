# objfilesymloc

`find` resolves a defined ELF symbol's file-backed bytes. Uses section-relative
st_value for ET_REL, virtual values minus section address for EXEC/DYN. Rejects
undefined, ABS/COMMON/reserved section indexes, NOBITS and section-crossing ranges.
Returns 0 success, negative ELF error; output unchanged on failure. Borrowed
result expires with immutable source. A zero-size symbol returns an empty span.

Not runtime dynamic symbol resolution or weak/global arbitration; lookup is
the first defined exact name as in elfutils.
Switches: `MULIB_IMPL_OBJFILESYMLOC`, `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_BINFILE`. Example: `examples/modules/objfilesymloc.c`.
