# Owned DocIR publication pipeline

This pipeline turns a semantic document into versioned DocIR, HTML/XHTML with
structural MathML, LaTeX source, and EPUB chapters. It is not a native TeX engine,
page-placement solver, CSL processor, or standards-certification claim.

## Build and verify

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/docir-publication.c -lm -o docir-publication
./docir-publication
make docir-formats
```

The complete switch recipe is in [the example](../examples/docir-publication.c).
Define `MULIB_ENABLE_DOCIR` to enable tree adapters in `docvalidate`,
`docnormalize`, `docserialize`, `docsource`, `latex`, `html`, and `epub`, along
with their documented implementation dependency switches. The core `docir`
tree/snapshot/render APIs need only `MULIB_IMPL_DOCIR` and `MULIB_IMPL_TYPECORE`.
Legacy APIs remain available without the feature flag and acquire no new link
dependencies; tree adapters return failure/unsupported when disabled.

`make docir-formats` independently reads the binary schema and generated XML/ZIP
with Python's standard library. When installed, pdfLaTeX compiles the article
twice with shell escape disabled; qpdf checks its PDF and link destination, and
pdftotext checks extracted text. These are regression checks, not PDF/A, PDF/UA,
EPUB, MathML Core, or TeX conformance certification.

## Ownership and ABI

The existing public `mulib_docir_node_t` layout and kind values 0..6 are unchanged.
Use `new`, `build`, `clone`, or the binary reader to create managed nodes. Node
fields, text, and child arrays are borrowed **read-only** views; do not shallow
copy nodes, construct their storage manually, or free their members directly.
The implementation tracks provenance and ownership outside the public layout.

`append` transfers one detached non-root child on success. It rejects an already
owned child, self-attachment, or an ancestor cycle without changing either tree.
`parent` and `owner` return borrowed pointers. `detach(parent,index)` transfers
ownership back to the caller. Destroying an attached child unlinks it safely.
`destroy` is iterative, allocation-free, and follows private ownership links;
corrupt public counts or aliased/cyclic public edges do not cause recursive
overflow or double freeing. Export/snapshot operations reject those corruptions.

Text setters validate UTF-8 and the supported XML 1.0 character range. Embedded
NULs are not semantic text. Text append supports self-aliasing and geometric
growth. Attribute/text/style allocation failures preserve the old value.
`set_id` accepts a bounded ASCII identifier; `set_target` rejects controls,
backslashes, protocol-relative URLs, and schemes other than HTTP/HTTPS/mailto.
Relative links are allowed; this is not a complete RFC 3986 or resource validator.

## Semantic subset

Stable kind values are:

| Values | Kinds |
| --- | --- |
| 0..6 | root, section, paragraph, text, link, list, item (legacy values) |
| 7 | heading (level 1..6) |
| 8..14 | math, math row, symbol, fraction, superscript, subscript, radical |
| 15..18 | internal reference, figure, caption, inline footnote |
| 19..23 | bibliography, bibliography item, citation, line break, inline code |

Lists contain item sequences (empty lists remain valid); bibliographies contain nonempty identified
bibliography items. Paragraphs/headings/captions/links contain supported inline
nodes, not nested block structures or links within links. A figure has at most
one caption, first or last. Fractions/scripts require two math children, radicals
and math roots one; math rows group math children. Symbols are literal tokens,
not raw TeX. Other mathematical syntax must be expressed with structural nodes.

`docvalidate_tree` checks a complete root, ownership, budgets, arities, duplicate
IDs, dangling internal references, and citation targets. Cross-document URL
resolution, image inclusion, automatic citation formatting/sorting, and figure/
footnote placement are not provided by this layer.

Styles store bold/italic/monospace flags and checked scaled-point sizes/indents
(65,536 sp = 1 pt; zero size inherits). HTML emits scoped typography properties;
LaTeX emits scoped text-font commands. This is not a complete CSS cascade or
OpenType MATH layout. Styles and IDs on math-internal nodes are rejected; style
the containing math/paragraph node instead.

`set_source` stores a copied filename and byte span, with 1-based byte line/column.
`docsource_bind` validates a span against a `typesrc` source. No files are read.
Normalization makes an independent tree, merging adjacent text siblings only
when styles match and neither carries IDs, links, or provenance. It preserves
whitespace and does not claim Unicode NFC/NFKC normalization.

## Snapshots, limits, and concurrency

`snapshot_make` creates an owned flat preorder snapshot with parent indices,
styles, provenance, and copied strings. It survives destruction of the tree.
Use `snapshot_destroy`; never free snapshot strings/records separately.
`snapshot_validate` optionally checks complete-document and reference invariants.
`render_snapshot` exports an immutable snapshot; HTML/LaTeX/EPUB adapters capture
once and validate before publishing output.

Default limits are 100,000 nodes, 16 MiB of copied string bytes, and depth 256.
Hard limits are 1,000,000 nodes, 64 MiB, and depth 4,096. Zero limit fields select
defaults. Traversal/destruction is iterative. Validation uses bounded workspaces
and sorting/binary search for IDs rather than quadratic reference scans.
Failure never publishes a partial tree/document or appends partial output.

Define compatible `MULIB_DOCIR_CALLOC`, `MULIB_DOCIR_REALLOC`, and
`MULIB_DOCIR_FREE` hooks in the implementation TU; snapshot string/output buffers
use the separate typecore hooks. Hooks must preserve normal allocation/alignment
and realloc-failure semantics and must not reenter DocIR. Native C11 atomics
protect the private registry. For MSVC/no-atomics implementations, provide paired
`MULIB_DOCIR_LOCK()`/`MULIB_DOCIR_UNLOCK()` hooks or serialize all DocIR calls.
Clients must still coordinate the lifetime of borrowed nodes. Snapshot consumers
must not concurrently modify their records or lie about backing-array lengths.

## MuDocIR binary version 1

This is a MuLIB-defined schema, not a claimed external standard. All integers are
big endian; no C layouts, pointers, padding, or floating-point values are stored.

1. Eight magic/version bytes: `MuDocIR` followed by byte `01`.
2. A nonzero unsigned 32-bit node count.
3. Preorder records: six 32-bit fields (kind, parent index, level, style flags,
   signed font-size sp, signed indent sp), then four unsigned 64-bit source fields
   (begin, end, line, column), then text/ID/target/source strings in that order.
4. Each string is a 32-bit byte length and UTF-8 bytes without NUL; `FFFFFFFF`
   denotes absent, distinct from an empty string. Signed values use two's-complement
   wire encoding; the root parent is `FFFFFFFF`. Non-heading levels must be one.

Decoders reject invalid kinds, parent/preorder relationships, UTF-8, source/style
metadata, sizes, budgets, trailing bytes, and noncanonical fields. Source values
that do not fit host `size_t` fail rather than truncate. Fragment references may
remain unresolved on read; complete publication adapters validate them before
export. No compression, integrity checksum, or unversioned extension is implied.

## Format contracts and tests

HTML output uses the XHTML namespace and a fixed HTML5 doctype; MathML uses its
own namespace and structural token/fraction/script/radical elements. The EPUB
adapter removes that fixed doctype (the existing XML parser still rejects all
DTDs), marks MathML chapters, and delegates packaging to **upstream `mulib-epub`**.
Required identifiers, language, date, creator, and existing media stay caller
controlled; there is no fixed production identifier or invented EPUB backend.

LaTeX articles explicitly require `article`, `amsmath`, and `hyperref`. The tested
ASCII profile compiles with TeX Live 2024 pdfLaTeX. Non-ASCII text requires an
appropriate UTF-8 engine/font setup; use the escaped fragment API with your own
preamble as needed. No raw-TeX escape hatch is exposed. Actual typesetting remains
the TeX engine's job, not a promise of full native TeX/LaTeX compatibility.

Regression sources: `tests/test-docir-pipeline.c`, `tests/test-docir-allocations.c`,
`tests/test_docir_threads.c`, and `tests/fuzz_docir.c`. Remaining typography,
compiler/expansion, page-builder, native rasterization, and normative Unicode/
format work in `AGENTS.md` is not completed by this semantic-document milestone.
