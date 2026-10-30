# objfile

Borrowed object model restricted to ELF ET_REL. `open` uses the existing checked
ELF32/64 parser and rejects executable/shared/other object formats. `section`
borrows bytes by exact name; NOBITS sections return ETYPE. Returns 0 success or
negative ELF errors; outputs unchanged on failure. Source owns all bytes and
must remain live/immutable while views are used.

No COFF/Mach-O, symbol linking, relocations, native execution or file I/O.
Switches: `MULIB_IMPL_OBJFILE`, `MULIB_IMPL_ELFUTILS`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_BINFILE`.
Example: `examples/modules/objfile.c` also generates an object using the writer.
