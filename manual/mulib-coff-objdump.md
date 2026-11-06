# COFF dump adapter

Include `mulib-coff-objdump.h`. Define `MULIB_IMPL_COFF_OBJDUMP`, `MULIB_IMPL_COFF`,
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`,
`MULIB_IMPL_UTF8`, `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID`.
[Example](../examples/modules/coff-objdump.c).

`mulib_coff_objdump(data, length, options, out)` opens the checked COFF reader
then appends shared-format text atomically, returning 1/0. `objdump_view` is the
same renderer for an already validated COFF view, including the view embedded
in PE. Views and backing bytes must remain unchanged during rendering.

HEADERS prints COFF machine/characteristics (COFF alone has no universal bitness
field, so bits=0). SECTIONS prints native section fields. SYMBOLS prints primary
records only, skipping auxiliaries while retaining their raw table indexes.
CONTENTS adds bounded file-backed hex dumps; BSS without file data is not read.
Long and short names are resolved and escaped; malformed names fail the dump.

[Shared options](mulib-objdump.md) control limits and optional terminfo headings.
Record-limit, parse, style and allocation failures leave existing output unchanged.
No instruction disassembly, auxiliary decoding, relocation rendering, archive
iteration or external tools are used. See [COFF profile](mulib-coff.md).
