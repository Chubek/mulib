# objfilewriter

`write` appends a deterministic ELF64 little-endian ET_REL object to a buffer.
Borrowed section inputs supply unique nonempty names, bytes, power-of-two
alignment and WRITE/ALLOC/EXECINSTR flags only. `.shstrtab` is reserved. Writer
emits PROGBITS sections, null section and section-name table; machine is supplied
as ELF e_machine (62 for x86-64). At most 65277 user sections.

Returns 1 success, 0 invalid/overflow/OOM; complete object built temporarily so
failure preserves output and inputs may alias it. No symbols, relocation tables,
program headers, NOBITS or extended counts. Never executes bytes.
Switches: `MULIB_IMPL_OBJFILEWRITER`, `MULIB_IMPL_BINFILEWRITER`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/objfilewriter.c`.
