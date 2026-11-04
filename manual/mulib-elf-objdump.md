# ELF dump adapter

Include `mulib-elf-objdump.h`. Define `MULIB_IMPL_ELF_OBJDUMP`, `MULIB_IMPL_ELF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF` before inclusion in one TU.
[Example](../examples/modules/elf-objdump.c).

`mulib_elf_objdump(data, length, options, out)` validates ELF32/ELF64 in either
byte order and appends a complete dump atomically. Returns 1 on success and 0
on malformed bytes, requested record limit, unavailable style or allocation error.
The output is an initialized owned string buffer; no input ownership is taken.

HEADERS prints file class/machine/type and program records; SECTIONS prints
section metadata; SYMBOLS prints all SYMTAB/DYNSYM records, including undefined
and local symbols. CONTENTS adds bounded hex dumps of file-backed non-null,
non-NOBITS sections. Missing section-name tables produce empty names. Invalid
present names or symbols are errors, not silently omitted records.

Options/defaults/escaping and limits are shared with [objdump](mulib-objdump.md).
Symbols are limited in aggregate across symbol tables. ELF's existing extended
counts/index handling is reused. There is no instruction disassembly, relocation
dump/application, DWARF decoding or external tool invocation. See
[ELF reader](mulib-elf.md) for structural reader scope.
