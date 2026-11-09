# mulib-tbl

[Manuals](README.md) · [Library index](index.md)

Rectangular tables on the shared XML-compatible documentation DOM. A table is a
`table` element containing `row` elements containing `cell` elements. Cells can
contain structured inline XML; `header="true"` and `align="left|center|right"`
carry presentation metadata. This is a portable table model, not a spreadsheet,
CSS layout engine, or spanning-cell implementation.

## Integration and ownership

Define TBL, DSDOM, XML, CSV, TEX, STRBUF and UTF8 implementation switches in one
translation unit. The [standalone example](../examples/modules/tbl.c) is complete.
`mulib_tbl_t` is the XML node type; append/detach/destroy follow
[XML ownership rules](mulib-xml.md). No third-party dependencies are needed.

`create` returns an owned empty table or NULL. `add_row` copies all literal fields
and appends only after the whole row succeeds; the returned row is borrowed.
First row fixes the width. Header is exactly 0/1; alignment entries are the public
enum values. Failure returns NULL without modifying the table. NULL fields are
invalid; empty fields are valid. `rows`/`columns` return zero on invalid roots;
`cell` returns a borrowed cell or NULL out of bounds. External XML edits can
invalidate borrows and table validity.

`validate` accepts only rectangular row/cell structure, valid alignment/header
flags, element/text content, at most 4096 rows, 256 columns, 100000 total nodes,
64 nested content edges and 8 MiB of names/text/attributes. `colspan`/`rowspan`
attributes are rejected, rather than silently flattened. Empty tables are valid;
nonempty rows cannot be empty. Its optional diagnostic is static, with offset a
row index for shape errors. Direct XML edits are validated before serialization.

## Readers and writers

`parse(source,length,format,error)` copies a `pipe` (default) or `csv` table from
at most 1 MiB of XML-valid UTF-8, without embedded NUL. NULL source at zero
length is allowed and produces an empty table. No partial table escapes on failure.

Pipe rows have optional outer pipes and trim surrounding spaces/tabs. Blank lines
are skipped. A separator immediately after the first row has at least three
dashes in every cell; optional leading/trailing colons set alignment. It marks
the first row as headers and applies column alignment to subsequent rows.
Repeated/misplaced separators and ragged rows fail. `\|` and `\\` decode to
literal pipe/backslash in standalone pipe input. Other markup stays literal.

`split` exposes owned raw fields and separator alignments for markup frontends;
it does **not** decode escapes or execute markup. Matched backtick runs, escaped
pipes and balanced groups protect internal pipes. Initialize output to zero,
free with `mulib_tbl_fields_free`, and free before reuse. Failure leaves output
unchanged. Nesting is at most 64, columns at most 256, with a bounded row scanner.
CSV uses [MuLIB's quoted-field dialect](mulib-csv.md), including quoted commas,
quotes and newlines. CSV does not infer header/alignment metadata.

`render` returns malloc-owned NUL-terminated XML, HTML, LaTeX, pipe, or CSV; length
excludes NUL and is zero on failure. XML preserves structured cell contents.
Other writers use descendant **text**, not raw markup: HTML escapes it and uses
th/td with alignment, LaTeX escapes text and chooses l/c/r columns, and CSV quotes every field.
CSV roundtrips text, not styling metadata. Pipe escapes pipes/backslashes, uses
first-row header/alignment separators, and rejects multiline cells; rich markup
and independently styled later headers are not a lossless pipe representation.
An empty table renders as empty CSV/pipe/LaTeX, an empty HTML table, or its XML tree.

## Docstrings integration

[`mulib-docstring.h`](mulib-docstring.md) uses this module's splitter/validator
for its rich inline pipe tables. `.table csv` or `.table pipe`, terminated by
`.endtable`, imports a literal table. Lua `ldocstr.table(source,format)` does the
same; existing `ldocstr.Table/Row/Cell` constructors remain available. Raw XML,
streams and DOM preserve cell attributes; HTML and Pandoc expose alignment.
No other new notation module is automatically connected to Docstrings.

Objects require caller synchronization; there is no global mutable state.
Readers/writers are bounded, primarily linear in content, with XML insertion and
row-width queries adding per-row scans. Evidence:
[notation tests](../tests/test_notation.c),
[failure injection](../tests/test_notation_alloc.c), and
[Docstrings tests](../tests/test_docstring.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-tbl.h`](../include/mulib-tbl.h) |
| Implementation switch | `MULIB_IMPL_TBL` |
| Repository-local includes | [mulib-csv](mulib-csv.md), [mulib-dsdom](mulib-dsdom.md), [mulib-tex](mulib-tex.md) |
| Complete example | [`examples/modules/tbl.c`](../examples/modules/tbl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tbl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
