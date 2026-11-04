# COFF reader

Include `mulib-coff.h`; define `MULIB_IMPL_COFF` and `MULIB_IMPL_ENDIAN` in one TU.
See [example](../examples/modules/coff.c) and [shared contracts](binary-formats.md).

`mulib_coff_open(&view, data, size)` borrows Microsoft little-endian COFF bytes.
`open_at` supports a COFF header embedded in PE; section/symbol offsets are still
relative to the original file, not the embedded header. The optional-header span
is bounded but opaque. COFF has no universal magic: a nonzero machine field is
accepted, so this function is not a reliable arbitrary-file format detector.

`section`, `section_name`, `symbol`, `symbol_name`, and `bytes` return decoded
records or borrowed bounded spans. Fixed eight-byte names need not end in NUL;
always use the returned length. `/decimal` section names and zero-prefix symbol
names resolve against the length-prefixed string table, with termination checked.
`symbol_count` counts raw 18-byte entries including auxiliaries. Iterate primary
symbols by `i += 1 + symbol.auxiliary_count`; accessing an auxiliary index fails.
Section indexes preserve signed COFF values (0 undefined, -1 absolute, -2 debug).

Open validates table and section payload/relocation/line-number ranges and auxiliary
counts. It does not apply relocations or decode auxiliaries/line records. BSS can
have a nonzero memory size with no file payload. Name strings and symbol section
indexes are validated on access. Archives, bigobj, import objects and relocation
overflow extensions are unsupported. Views must remain unmodified.

Results: `OK=0`, `EINVAL=-1`, `EFORMAT=-2`, `EBOUNDS=-3`, `EUNSUPPORTED=-4`,
`ENOTFOUND=-5`. There is no allocation, hidden error state or file I/O.
