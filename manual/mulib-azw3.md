# MuLIB AZW3: KF8 sections and flows

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_AZW3`, `MULIB_IMPL_MOBI` and `MULIB_IMPL_BUFFER` in one
translation unit, then include `mulib-azw3.h`. No external libraries.
[Complete example](../examples/modules/azw3.c).

## Supported profile

This module reads unencrypted KF8/MOBI version 8 and MOBI 6/7 hybrids containing
an EXTH 121 boundary reference. For hybrids it verifies the exact `BOUNDARY`
record and selects the following version-8 header. Compression and encoding
support are inherited from [MOBI](mulib-mobi.md): uncompressed/PalmDOC text,
UTF-8 or Windows-1252 bytes. Unsupported compression and encrypted files fail.
Duplicate EXTH 121 fields and boundaries inside legacy text records are rejected.
The `0xffffffff` boundary sentinel denotes a book without a KF8 section and
returns `EUNSUPPORTED`.

For multiple flows, the reader validates the FDST signature, 12-byte header,
flow count, and every start/end offset. Ranges must be contiguous, ordered,
nonoverlapping and cover the declared decoded text exactly. A header flow count
of zero or one denotes one whole-text flow; the FDST pointer is ignored in this
case, matching KF8's single-flow convention.
An FDST record must follow the selected section's text records and contain exactly
the declared table, with no truncated or undeclared trailing entries.

**Flows are raw KF8 data**, which can include markup fragments, CSS and embedded
SVG. The module does not rebuild chapters from skeleton/fragment INDX records,
rewrite Kindle URIs, decode fonts/images, render pages, create AZW3 books, or
support KFX, Topaz, Print Replica or DRM. The underlying `reader.mobi` and
`mulib_mobi_record` expose opaque records for applications that need them.

## API, ownership and errors

`mulib_azw3_open(&reader, data, size)` allocates nothing. It borrows immutable
archive bytes and changes the reader only on success. No cleanup is needed;
readers may be copied as long as the input remains live.

`mulib_azw3_text` appends the complete decoded text. `mulib_azw3_flow_range`
returns a zero-based flow's offset/length in that text; outputs change only on
success. `mulib_azw3_flow` decodes all text and appends the requested slice.
Its limit bounds **total decoded text**, not merely the requested flow, so a
small slice cannot bypass the memory limit. For repeated access, decode once
and use flow ranges. All buffers are binary and not implicitly terminated.

Results use `MULIB_MOBI_*` codes, zero on success. A non-KF8 book returns
`EUNSUPPORTED`; missing/malformed hybrid boundary or FDST structure returns
`EFORMAT`. Invalid indices/pointers return `EINVAL`. Buffer writes are atomic
on failure; readers inherit MOBI database limits. Decode time/memory is linear
in all decoded text. If output aliases input, growth invalidates borrowed views.

`tests/test_ebook_formats.c` covers standalone and hybrid selection, two flows,
truncation at every byte, invalid ranges and bounded extraction.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-azw3.h`](../include/mulib-azw3.h) |
| Implementation switch | `MULIB_IMPL_AZW3` |
| Repository-local includes | [mulib-mobi](mulib-mobi.md) |
| Complete example | [`examples/modules/azw3.c`](../examples/modules/azw3.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/azw3.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
