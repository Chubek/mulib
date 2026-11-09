# MuLIB PDF: deterministic page generation

[Manuals](README.md) · [Library index](index.md)

## Integration and scope

Define `MULIB_IMPL_PDF`, `MULIB_IMPL_BUFFER` and `MULIB_IMPL_STRBUF` in one
translation unit and include `mulib-pdf.h`. The module uses only ISO C11;
no printer driver, external converter, PDF library or graphics backend is needed.
[Complete example](../examples/modules/pdf.c).

Output is PDF 1.4 with a catalog, flat page tree, standard Type 1 Helvetica font,
uncompressed page-content streams, classic cross-reference table and trailer.
It supports independently sized pages, printable ASCII text, RGB fill/stroke
colors, lines and filled/stroked rectangles. It does not parse existing PDFs,
shape/wrap text, embed fonts/images, perform pagination, or implement encryption,
annotations, tagged PDF or PDF/A. These are explicit API boundaries, not implicit
conformance claims. Text outside printable ASCII is rejected.

## Lifecycle and drawing

Zero-initialize `mulib_pdf_t`. `mulib_pdf_add_page` appends a page and returns its
stable zero-based index through an output pointer. Page dimensions are
0.001..14400 points. Pages start black; `mulib_pdf_color` changes both fill and
stroke colors for later operations on that page. RGB components must be finite
and in 0..1. Text does not change drawing colors.

Coordinates use points (1/72 inch), bottom-left origin, positive x rightward,
positive y upward. They must be finite and within +/-1000000; off-page content
is allowed. Numeric serialization rounds to 0.001 point with locale-independent
decimal notation. Text size is 0.001..1000 points. Line widths are 0..1000;
zero denotes a PDF hairline. Rectangle dimensions are nonnegative and at most
1000000. `fill` is exactly 0 or 1. Line/rectangle calls set the page's line width.

`mulib_pdf_text` copies the specified byte range, validates ASCII 32..126 and
escapes parentheses/backslashes so content cannot become PDF operators.
`(NULL,0)` emits an empty text operation; tabs/newlines/embedded NUL are rejected.
The caller chooses baselines and handles line breaking.

## Serialization, results and ownership

`mulib_pdf_write(document, out)` appends a complete standalone PDF. At least one
page is required. Offsets are relative to the start of that appended PDF, not
an existing output prefix. Output is deterministic: no clock, random IDs or
locale-dependent numbers. Repeated serialization is supported and does not
freeze the document. `mulib_pdf_free` releases/reset pages; NULL and repeated
free are harmless. Live documents must not be copied or have fields modified.

All integer APIs return zero on success, or `EINVAL`, `ENOMEM`, `ELIMIT`.
All modifications preserve logical state/content on failure. Drawing data is
copied during calls; no input pointers are retained. Page indices remain stable
across growth, but internal pointers do not. Output uses an ordinary owned
`mulib_buffer_t`. Maximum page count is 100000; serialized object offsets are
limited to 32-bit range. Serialization uses O(total content) scratch memory.
Content totals are checked before allocating/copying serialization scratch,
and each stream is checked against the remaining offset range before copying.
Drawing/serialization time is proportional to emitted bytes. Each document
requires caller synchronization if shared between threads.

## Verification

`tests/test_ebook_formats.c` covers text escaping, coordinates, lifecycle and
deterministic output; `tests/test_ebook_alloc.c` checks failure atomicity.
`tools/check_ebook_formats.py` independently checks xref offsets/stream lengths
and, when available, uses Poppler `pdfinfo`/`pdftotext` to verify page count and
extracted text.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-pdf.h`](../include/mulib-pdf.h) |
| Implementation switch | `MULIB_IMPL_PDF` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-strbuf](mulib-strbuf.md) |
| Complete example | [`examples/modules/pdf.c`](../examples/modules/pdf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/pdf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
