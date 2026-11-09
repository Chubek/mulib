# MuLIB EPUB: compressed EPUB 3 publications

[Manuals](README.md) · [Library index](index.md)

## Integration

Include `mulib-epub.h`. In exactly one translation unit define
`MULIB_IMPL_EPUB`, `MULIB_IMPL_ZIP`, `MULIB_IMPL_DEFLATE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_XML`, `MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8`.
Compression and packaging are provided by MuLIB's [DEFLATE](mulib-deflate.md)
and [ZIP](mulib-zip.md) modules. No third-party runtime dependencies.
[Complete example](../examples/modules/epub.c).

## Publication generation

`mulib_epub_write(book, out)` creates a deterministic EPUB 3 ZIP with:

- First, uncompressed `mimetype` containing exactly `application/epub+zip`.
- `META-INF/container.xml` referring to `EPUB/package.opf`.
- OPF metadata, manifest and spine; a generated EPUB navigation XHTML document.
- DEFLATE-compressed caller resources under `EPUB/`.

The book requires nonblank identifier, title and language, a valid Gregorian UTC
timestamp `YYYY-MM-DDThh:mm:ssZ`, and at least one spine resource. Creator is
optional. Metadata is UTF-8 XML text and is escaped. Language values are copied;
BCP47 language-tag semantics are caller-supplied. No clock is consulted.

Resources provide a relative path, media type, bytes, optional TOC title and
optional properties. A non-NULL title places an XHTML resource in the spine and
navigation, in input order. Resource paths must be unique; generated `package.opf`
and `nav.xhtml` are reserved. Properties support space-separated `mathml`, `svg`
and `cover-image`. XHTML resources are parsed as XML and require an `html` root
in the XHTML namespace. Media types require a type/subtype token pair; at most
one `cover-image` is permitted, and it must have an image media type.
Other media payloads are copied as opaque bytes.
The caller supplies XHTML content, CSS, images and their internal links; the API
does not paginate, transform source text into XHTML or automatically link styles.

Path components use ASCII letters/digits, `-`, `_`, `.`, `~` and `/`. Absolute,
empty, dot and dot-dot components, URI escapes, fragments, queries and directory
paths are rejected. These restrictions avoid ambiguous URL/path resolution.

## Opening and reading

Zero-initialize `mulib_epub_reader_t`, then call `mulib_epub_open(reader, data,
size, xml_limit)`. The archive is borrowed and must stay live/immutable until
`mulib_epub_close`. Open owns a ZIP index, parsed package DOM, copied package
path, manifest item array and spine-index array. Failure preserves a fresh
reader; opening an already-live reader fails. Close frees owned memory/reset
state and is safe to repeat.

The reader supports one-rootfile EPUB 3.0 packages with
stored/DEFLATE ZIP32 resources. It verifies the OCF mimetype placement/content,
container/package structure, unique manifest ids and paths, existence of local
resources, exactly one XHTML navigation item, and XHTML spine references.
Repeated spine entries, duplicate structural sections, unexpected rootfile
elements, non-whitespace text between structural elements and non-XHTML spine
content are rejected. Required Dublin Core identifier/title/language text is
checked, `unique-identifier` must select exactly one identifier, and exactly one
publication-level modified timestamp must contain a valid Gregorian UTC date.
Manifest identifiers use the XML module's ASCII name grammar without colons.
Metadata is preserved in `reader.package` for XML traversal; this API is not a
complete EPUBCheck replacement and does not validate resource markup, language-tag
semantics, navigation anchors or all EPUB content rules at open time.

EPUB 2, multiple renditions/rootfiles, encryption/obfuscation declarations,
ZIP64 and external manifest URLs are outside the profile. The XML parser rejects
DTDs and unsupported processing instructions. OCF, OPF, Dublin Core and XHTML
element names resolve both default namespaces and explicit prefixes through
ancestor declarations, including rebinding. The portable path rules above
also apply to package paths and manifest hrefs. `META-INF/encryption.xml` yields
`EUNSUPPORTED`, including font-obfuscation-only declarations.

`reader.items[i]` exposes borrowed package strings and ZIP entry indices;
`reader.spine[i]` indexes this array in reading order. Optional `linear=no`
items remain in this array; inspect the package DOM if that distinction matters.
`mulib_epub_find` returns an exact id's index or `SIZE_MAX`.
`mulib_epub_read(reader, item, limit, out)` decompresses one item, verifies its CRC,
and atomically appends its bytes. All borrowed pointers expire on close.

## Limits, errors and complexity

Zero is success. Negative errors are `EINVAL`, `EFORMAT`, `EUNSUPPORTED`,
`ENOMEM`, `ELIMIT`, `ENOTFOUND` (reserved for lookup-style extensions).
ZIP integrity/CRC failures map to `EFORMAT`. Explicit XML allocation diagnostics
map to `ENOMEM`; other XML errors map to `EFORMAT`. `find` uses `SIZE_MAX`.
Lengths are bytes without terminators. Empty opaque resources accept `(NULL,0)`.
Outputs use initialized `mulib_buffer_t` objects and must eventually be freed.
Write/read preserve output content on every failure. Writing copies inputs during
the call and allows input/output overlap; extracting into archive storage can
invalidate the reader and must not be followed by further reads.

`xml_limit` independently bounds decompressed container and package XML;
resource extraction uses a separate mandatory limit. ZIP32 limits apply, with
at most 65530 caller resources when generating. ID/path/reference validation and
writer duplicate checks can be quadratic in resource count. Serialization uses
scratch space proportional to generated content; extraction buffers one decoded
resource. There is no filesystem I/O, network access or mutable global state.

## Verification

`tests/test_ebook_formats.c` covers compressed roundtrip, metadata escaping,
limits, truncation and malformed OCF data. `tests/test_ebook_alloc.c` injects
allocation failures during generation/opening. `tools/check_ebook_formats.py`
checks ZIP integrity with Python `zipfile`, inspects namespaced OPF/XML and reads
an EPUB repacked with zlib-generated DEFLATE streams. It also checks rejection
of duplicate ids, missing spine references, wrong namespaces and unsafe hrefs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-epub.h`](../include/mulib-epub.h) |
| Implementation switch | `MULIB_IMPL_EPUB` |
| Repository-local includes | [mulib-xml](mulib-xml.md), [mulib-zip](mulib-zip.md) |
| Complete example | [`examples/modules/epub.c`](../examples/modules/epub.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/epub.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
