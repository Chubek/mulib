# Shared object/binary dump tools

Include `mulib-objdump.h`; define `MULIB_IMPL_OBJDUMP`, `MULIB_IMPL_TERMSTYLE`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_CSS`, `MULIB_IMPL_XML`,
`MULIB_IMPL_UTF8`, `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID`.
[Example](../examples/modules/objdump.c). The additional switches supply
termstyle's CSS/shared-cell APIs; existing indexed styling is unchanged.

This is the common rendering layer, not a file-format detector or disassembler.
`header`, `section`, `symbol`, `name` and `hex` append deterministic text to an
initialized `mulib_strbuf_t`. Each call is atomic: 1 success, 0 invalid input,
overflow, unavailable style capability or OOM, leaving existing output intact.
Input bytes/names may alias the output because rendering uses temporary storage.

`name` escapes controls, non-ASCII and backslash as `\xHH`, preventing embedded
terminal escape sequences in untrusted file names. `hex` renders 16-byte rows
with hexadecimal offsets and printable ASCII; nonprintable ASCII becomes `.`.
Its limit caps input bytes rendered; omitted bytes produce a truncation marker.
Empty NULL input is accepted; displayed offsets must not overflow uint64_t.

`mulib_objdump_options_init` selects HEADERS|SECTIONS|SYMBOLS, max_bytes=256,
max_records=4096 and no styling. Format adapters accept NULL for these defaults.
CONTENTS adds hex data and section labels even without SECTIONS. Limits of zero
mean zero, never unlimited. Too many requested records fail atomically rather
than silently omitting records. Symbol limits are aggregate for ELF; other tables
are limited separately. These limits do not cap total output allocation: long
names and many sections can produce much more output than max_bytes.

Optional terminfo-backed bold styling affects only format headings. Plain output
contains no terminal controls from names. Applications choose the correct adapter:
[ELF](mulib-elf-objdump.md), [COFF](mulib-coff-objdump.md),
[PE](mulib-pe-objdump.md), [Mach-O](mulib-macho-objdump.md).
No file I/O, machine-code decoding, demangling or relocation application occurs.
