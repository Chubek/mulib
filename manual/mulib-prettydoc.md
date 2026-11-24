# `mulib-prettydoc.h`: structured code-formatting documents

## Scope and build

C11 kernels; no ICU installation is required for PrettyPrint. Use the MuLIB
planner for one implementation TU. Standalone examples use
`kernels/mulib-pretty-standalone.inc` to select the switches. On Linux link
with `-lm -ldl`.

Unlike MuLIB's glyph-oriented `mulib-layout.h`, this kernel operates on *code structure*: `Text`, `SoftLine`, `HardLine`, `Concat`, `Group`, `Nest`, `Align`, `Choice`, `Annotate`, `Fill`, `Region` (local algorithm selection), and `ParagraphRegion` (local paragraph-strategy selection).

## Algorithms

- `MULIB_PRETTY_GREEDY` (mode 0): wrap on softline based on whether the following text fragment fits. This local greedy heuristic is **not** a complete Oppen-style streaming algorithm.
- `MULIB_PRETTY_FITS` (mode 1): Wadler-style fit test for the candidate flattened group; break otherwise.
- `MULIB_PRETTY_SEARCH` (mode 2): try up to `2^N` combinations of the first `N` encountered group choices, with `N <= 12`, and choose the lowest scored layout; fallback to fit tests when the group count exceeds the limit. This is **not** Bernardy's or PrettyExpressive's full optimality algorithm.
- `MULIB_PRETTY_WORD_GREEDY` and `MULIB_PRETTY_MIN_RAGGEDNESS` select greedy
  wrapping or minimum raggedness for `Fill`. The separate
  `MULIB_PRETTY_KNUTH_PLASS` mode uses the global
  [linebreak optimizer](mulib-linebreak.md): boxes, stretch/shrink glue,
  fitness transitions and flagged discretionary hyphens. It respects
  distinct first-line and continuation widths, including ribbon limits.
  Select it with `--paragraph knuth-plass` or `(paragraph "knuth-plass" A)`.
  Literal hyphens, soft hyphens and zero-width spaces allow breaks;
  unselected soft hyphens are invisible. Infeasible paragraphs fail instead
  of silently switching algorithms. Glue ratios choose breaks but output
  uses natural single spaces; there is no physical glyph justification or
  automatic hyphenation dictionary.

`search_group_limit` defaults to 9. Excess group choices fall back to fit-based layout. The group pre-pass, fit checks and search candidates share a 4,194,304-step budget for node visits, scanned scalars/separators and paragraph trials. On budget exhaustion the call fails without publishing a partial layout. No universal global-optimality guarantee is claimed. The cost is `1e6*overflow-columns + sum(non-final-line remaining-width^2) + 8*line-break-count`; the final line's overflow is included. This objective is a heuristic, not a configurable general optimization language yet.

`mulib_pretty_options_t.ribbon_width` limits how much content fits after the indentation emitted by the layout engine. The effective line limit is the smaller of page width and the emitted indentation plus the ribbon width. The first line starts with zero emitted indentation. Changing the nest level mid-line does not change that line's ribbon. The default `0` disables the ribbon; `ppfmt --width 80 --ribbon 60` enables it. Ribbon width affects layout decisions, not a hard output cap: an indivisible word may still overflow.

Page width, ribbon width, alignment and `max_columns` use locale-independent
Unicode 16.0 scalar widths: nonspacing/enclosing marks and format characters
count as zero; other assigned Wide/Fullwidth scalars count as two; all others
count as one. Newlines break lines, and tabs in `Text` count as one (no tab
stops). `Fill` normalizes ASCII whitespace. The byte output cap and annotation
offsets/lengths still count UTF-8 bytes. This is not grapheme-aware terminal
width: emoji sequences, variable-width fonts and shaping can differ.
`python3 tests/generate_pretty_width.py` verifies the embedded table with
Python's Unicode 16.0 database; use `--write` to regenerate it.

## Input format

```scheme
(group
 (cat (text "call(")
      (cat (nest 2 (cat (soft) (text "alpha,beta")))
           (cat (soft) (text ")")))))
```

Accepted document operators: `(text "string")`, `(verbatim "string")`, `(fill "paragraph")`, `(soft)`, `(hard)`, `(cat A B)`, `(group A)`, `(nest COUNT A)`, `(align A)`, `(choice A B)`, `(annot "style" A)`, `(region "greedy|fits|search" A)`, `(paragraph "greedy|ragged|knuth-plass" A)`, `(source ID START END A)` and the math forms below. Strings accept escapes `\n`, `\r`, `\t`, `\\`, `\"` and `\xHH` (exactly two hexadecimal digits encoding one byte). Decoded text must be valid NUL-free UTF-8. The full input must be consumed; source is limited to 2 MiB, each decoded string to 1 MiB, parser depth to 128, and nodes to 200,000.

`Verbatim` preserves literal line endings without adding the active nest's
indentation after embedded LF. Those line breaks still count in layout,
inherit annotations and source spans, and are escaped by markup emitters.

Semantic styles: `normal`, `keyword`, `identifier`, `number`, `operator`, `comment`, `string`, `math-ident`, `math-number`, `math-operator`.

All adapters (including languages other than On1x) can serialize the same format, or directly call the document-builder C API. The kernel does *not* parse arbitrary programming languages: each frontend maps its AST/CST/IR into the portable document algebra.

## Render targets

- `MULIB_PRETTY_PLAIN` emits the text used for layout, including a linear representation of semantic equations.
- `MULIB_PRETTY_HTML_MATHML` emits standalone HTML5. Token annotations still map to `<mi>`, `<mn>` and `<mo>`; semantic equations use nested Presentation MathML.
- `MULIB_PRETTY_LATEX` emits a standalone article and loads `amsmath` for semantic equations. The TeX engine/font determines Unicode glyph support.
- `MULIB_PRETTY_TROFF` emits monospaced source with leading-dot/apostrophe protection and semantic equations in `.EQ`/`.EN` blocks (process with `groff -e`). Display blocks add backend-specific line boundaries.

Text is escaped for its target after layout. All targets use the same break decisions.

`mulib_pretty_layout` requires a zero-initialized output and returns 1/0. It leaves output untouched on failure; release text, pieces and maps with `mulib_pretty_layout_free`. `mulib_pretty_emit` and `mulib_pretty_render` return `malloc`-owned UTF-8 strings, or NULL on failure. `mulib_prettydoc_free` owns every document node/text. Node handles are valid only in the owning document. Rebuild C callers after the new map fields in `mulib_pretty_layout_t`.

## Semantic math

Use `(math-atom "text")` for a nonempty single-line atom, then `(frac A B)`,
`(sup A B)`, `(sub A B)`, `(root A)`, or
`(matrix (row A B) (row C D))`. Operands must be math nodes (optionally
wrapped in `source`); matrices must be rectangular with 1–32 rows and columns.
An equation is indivisible for wrapping; width is measured in its plain
linear representation, not in typeset glyph advances.

## Source maps

`(source ID START END A)` associates a half-open source byte span with the
text produced by `A`. Source IDs are caller-defined unsigned 32-bit integers.
The innermost source wins. Each entry in `layout.map` relates literal
UTF-8 bytes to a whole span, not individual source characters. Synthetic
indentation, breaks, normalized spaces, discretionary hyphens and math
punctuation are unmapped. Maps are capped at 65,536 entries.
`ppfmt --map FILE` writes JSON rows
`[outputStart,outputLength,sourceId,sourceStart,sourceEnd]` in **plain UTF-8**
coordinates even when the target uses markup.

## Incremental updates

Create a `mulib_pretty_session_t` borrowing an append-only document. Pass
independent block roots to `mulib_pretty_session_update`. On an edit, append
changed nodes and submit the new root list. The session reports how many
blocks it reused by root ID and identical options, including moved blocks.
Each block starts on a fresh line; one synthetic newline separates them.
The combined text and rebased map are rebuilt, but cached blocks are not
laid out again. Updates are transactional and capped at 1,024 blocks.
The view is borrowed until the next successful update or session destruction.
This is block-incremental C layout, not arbitrary subtree or editor reuse.

## Compiling formatter specs

[`ppgc.py`](../examples/ppgc.py) compiles JSON `.ppg` v1 to a standalone
Python formatter. Tagged JSON arrays are AST nodes. Rules match literal
tags, exact array shapes and scalar values, with named `{"capture":"name"}`
patterns. The first match for a tag wins. `{"render":"name"}` recursively
formats a captured descendant; `{"value":"name"}` substitutes a checked
string or unsigned 32-bit number. Unknown tags, wrong capture types and
budget exhaustion fail. Language-specific AST decoding is external.

```sh
python3 examples/ppgc.py examples/arithmetic.ppg > formatter.py
python3 formatter.py examples/arithmetic.ast.json |
  build/prettyprint/ppfmt --target html --map arithmetic.map.json
```

## Example native build

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Ikernels \
  examples/prettyprint-demo.c -lm -ldl -o /tmp/pretty-demo
/tmp/pretty-demo
```

`make check-prettyprint` builds the `ppfmt` document renderer and runs
the native kernel, CLI and cross-TU bridge tests. Use `PRETTY_DIR=...` to keep
artifacts separate. For planner-driven On1x and Exotract builds, see
[`std.prettyprint`](../on1x/stdlib/prettyprint/README.md#linking).

## Hierarchical algorithm composition

Different algorithms can operate in one document via nested `Region` and `ParagraphRegion` nodes. For example:

```scheme
(cat (region "fits" (group (cat (text "f(") (cat (soft) (text "x)")))))
     (cat (hard) (paragraph "ragged" (fill "A paragraph embedded in a comment."))))
```

The layout engine preserves the shared indentation and current column across region boundaries. A region cannot automatically optimize its parent's entire layout: search is bounded and only considers enumerated group decisions.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-prettydoc.h`](../kernels/mulib-prettydoc.h) |
| Implementation switch | `MULIB_IMPL_PRETTYDOC` |
| Repository-local includes | [mulib-linebreak](mulib-linebreak.md) |
| Complete example | [`examples/prettyprint-demo.c`](../examples/prettyprint-demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/prettyprint-demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
