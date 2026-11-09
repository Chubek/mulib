# MuLIB MOBI: PalmDB records and decoded ebook text

[Manuals](README.md) · [Library index](index.md)

## Integration and supported profile

Define `MULIB_IMPL_MOBI` and `MULIB_IMPL_BUFFER` in one translation unit and
include `mulib-mobi.h`. No external dependencies.
[Complete example](../examples/modules/mobi.c).

The reader accepts PalmDOC `TEXtREAd` databases and unencrypted `BOOKMOBI`
books with MOBI version 6, 7 or 8, book type 2 and encoding 1252 or 65001.
Compression 1 (uncompressed) and 2 (PalmDOC LZ77) are implemented. HUFF/CDIC,
DRM/encryption, resource databases, chained record lists, other versions,
dictionary-specific book types and other encodings return explicit errors.
This is a binary inspection/text extraction API, not an ebook renderer or writer.

PalmDOC decoding implements literal bytes, literal runs, space/character pairs,
and overlapping backward references with a per-record history. MOBI trailing
data entries use backward variable-length lengths and the optional multibyte
overlap suffix. Trailers are removed before decompression. The concatenated
decoded length must exactly match the PalmDOC header length; undeclared padding
or contradictory text lengths are malformed.
The declared `record_size` must be nonzero when text records exist, the total
text must fit their combined declared capacity, and each record's decoded length
is bounded by `record_size`. A resource index cannot point into the header/text
records. These checks apply before publication of a reader or decoded output.

## Reader lifecycle and metadata

`mulib_mobi_open(&reader, data, size)` validates the PalmDB record table and the
first header. It allocates nothing and changes the output only on success.
`mulib_mobi_select(&reader, absolute_record, &other)` selects another MOBI header
within the validated database, including in-place selection. This supports the
AZW3 hybrid reader. Other record contents are validated when consumed.

All reader fields are read-only views. Input must stay immutable/live while any
reader or span is used. Reader structs may be copied; no destroy function is
needed. `mulib_mobi_record` exposes any absolute database record, not just text.
`title` and `mulib_mobi_metadata(type, occurrence)` return borrowed binary spans,
never implicitly terminated strings. Metadata is searched by numeric EXTH type;
occurrence is zero-based, so repeated authors are accessible. Unknown EXTH types
are retained as raw values. No implicit UTF-8/Windows-1252 transcoding occurs.

The public fields identify text record count, selected header index, encoding,
compression, version, total text length and first-resource absolute index.
For PalmDOC, version is zero and title is the database name; the encoding hint
defaults to 1252. Resource payloads remain opaque.

## Results, limits and output ownership

All integer APIs return `MULIB_MOBI_OK` (0), `MULIB_MOBI_NOTFOUND` (1, metadata
absence), or negative `EINVAL`, `EFORMAT`, `EUNSUPPORTED`, `EENCRYPTED`, `ELIMIT`,
or `ENOMEM`. Record/metadata outputs remain unchanged on errors and absence.

`mulib_mobi_palmdoc_decode(data, size, limit, out)` appends one decoded record;
`mulib_mobi_text(reader, limit, out)` appends concatenated text. Both build a
temporary result and preserve output on failure. Limit applies to new output,
not an existing prefix. `(NULL,0)` is a valid empty compressed record. Buffers
are initialized/freed with the buffer API; input may alias output, though growth
can invalidate borrowed readers. Text is raw markup in the declared encoding,
not stripped plain text or reconstructed KF8 XHTML.

Databases are limited to `UINT32_MAX` bytes and 65535 records by the wire format.
The record table is checked in linear time; record access is constant-time;
metadata lookup is linear in EXTH count. Decoding uses memory proportional to
decoded text plus one temporary record and enforces the caller's hard limit.

## Verification and references

`tests/test_ebook_formats.c` uses original synthetic PalmDB/MOBI/KF8 records for
metadata repetitions, encryption rejection, malformed lengths, PalmDOC overlap,
trailer stripping and hybrid selection. `tests/test_ebook_alloc.c` covers
allocation failures. Field layouts were cross-checked against the public
[KindleUnpack format descriptions](https://github.com/kevinhendricks/KindleUnpack/blob/master/lib/mobi_header.py);
the C implementation and fixtures are original, with no vendored decoder code.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-mobi.h`](../include/mulib-mobi.h) |
| Implementation switch | `MULIB_IMPL_MOBI` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/mobi.c`](../examples/modules/mobi.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mobi.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
