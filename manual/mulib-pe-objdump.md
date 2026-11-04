# PE dump adapter

Include `mulib-pe-objdump.h`. Define `MULIB_IMPL_PE_OBJDUMP`,
`MULIB_IMPL_COFF_OBJDUMP`, `MULIB_IMPL_PE`, `MULIB_IMPL_COFF`, `MULIB_IMPL_ENDIAN`,
`MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_TERMINFO`,
`MULIB_IMPL_STRBUF`. [Example](../examples/modules/pe-objdump.c).

`mulib_pe_objdump(data, length, options, out)` opens PE32/PE32+ and atomically
appends a complete text dump. It returns 1 on success and 0 on malformed input,
limits, unavailable style capabilities or allocation failure. Existing output
remains unchanged on failure. No input ownership is taken.

HEADERS prints class/machine/characteristics, an `image` metadata record (address
is image base, offset is entry RVA, size is image size, flags is subsystem), and
nonempty data-directory records. Certificate directory 4 uses a file offset;
other directories are reported as RVAs, not decoded or resolved.
SECTIONS, SYMBOLS and CONTENTS reuse the COFF renderer. Symbols are COFF debug
symbols, **not DLL exports**. Contents include raw section padding because the
dump describes actual file data; virtual zero-fill is not synthesized.

See [shared options and escaping](mulib-objdump.md), [COFF adapter](mulib-coff-objdump.md)
and [PE reader scope](mulib-pe.md). No imports/exports, instructions, CLR, resources,
unwind data, relocation processing or signature validation are implemented.
