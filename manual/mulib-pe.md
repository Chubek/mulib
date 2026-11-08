# PE reader

[Manuals](README.md) · [Library index](index.md)

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
GlobalPtr directory 8 permits a nonzero RVA with the required zero size.

Import/export/resource decoding, relocation application, CLR metadata, debug
formats, signatures and execution are not implemented. COFF symbols may be absent
in stripped images. No native Windows headers are needed. See
[shared contracts](binary-formats.md) and [PE lookup](mulib-peutils.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pe.h`](../include/mulib-pe.h) |
| Implementation switch | `MULIB_IMPL_PE` |
| Repository-local includes | [mulib-coff](mulib-coff.md) |
| Complete example | [`examples/modules/pe.c`](../examples/modules/pe.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pe.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
