# PE reader

Include `mulib-pe.h`; define `MULIB_IMPL_PE`, `MULIB_IMPL_COFF`,
`MULIB_IMPL_ENDIAN`. [Example](../examples/modules/pe.c).

`mulib_pe_open` validates MZ/e_lfanew, PE signature, COFF tables, PE32/PE32+
optional-header minimums, directory-table bounds, power-of-two alignments,
header size, image bounds and file-backed sections. It borrows bytes and uses
[COFF result codes and ownership](mulib-coff.md). This is inspection, not a
Windows loader or complete Windows image-verification policy.

The view exposes bitness, image base, entry RVA, image/header sizes, alignments,
subsystem and the embedded COFF view. `section`/`section_name` use zero-based
indexes and return native virtual/raw sizes and offsets.
`symbol`/`symbol_name` expose optional COFF debug symbols by raw table index,
skipping auxiliary indexes as in the COFF reader. `bytes` maps file offsets,
whereas `rva` maps virtual addresses. `MULIB_PE_*` result spellings alias the
corresponding COFF codes.

`mulib_pe_rva` maps a contiguous **file-backed** range. It rejects missing ranges,
virtual zero-fill, address overflow, and overlapping mappings encountered during
resolution. Header RVAs map to file headers. A successful map borrows bytes.
`mulib_pe_directory` returns address/size pairs, including absent zero/zero
entries. All addresses are RVAs except certificate directory 4, whose address is
a file offset. Certificate ranges are checked at open; other directory payloads
are opaque and can be mapped explicitly using `rva`.

Import/export/resource decoding, relocation application, CLR metadata, debug
formats, signatures and execution are not implemented. COFF symbols may be absent
in stripped images. No native Windows headers are needed. See
[shared contracts](binary-formats.md) and [PE lookup](mulib-peutils.md).
