# Mach-O dump adapter

Include `mulib-macho-objdump.h`. Define `MULIB_IMPL_MACHO_OBJDUMP`,
`MULIB_IMPL_MACHO`, `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_OBJDUMP`,
`MULIB_IMPL_TERMSTYLE`, `MULIB_IMPL_TERMINFO`, `MULIB_IMPL_STRBUF`.
[Example](../examples/modules/macho-objdump.c).

`mulib_macho_objdump(data, length, options, out)` accepts the checked reader's
thin Mach-O32/64 profile in either byte order. A complete dump is appended
atomically to an initialized `mulib_strbuf_t`; 1 success, 0 malformed input,
record limit, style or allocation failure. Inputs are borrowed only for the call.

HEADERS prints class, CPU, file type and bounded load-command metadata records.
Unknown commands are reported by numeric type/size without interpreting their
payload. SECTIONS prints section metadata; SYMBOLS prints all nlist records,
including debug and undefined symbols. CONTENTS adds bounded file-backed hex
data, skipping zero-fill section types. Names are safely escaped by the common
renderer; fixed-size sixteen-byte names are supported without a source NUL.

See [options/defaults/limits](mulib-objdump.md) and [reader scope](mulib-macho.md).
No fat-file traversal, machine-code disassembly, dyld fixups, relocation records,
debug information or signature verification. No external process is launched.
