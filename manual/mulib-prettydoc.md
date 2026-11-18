# `mulib-prettydoc.h`: structured code-formatting documents

## Scope and build

C11-only, no third-party libraries. Define `MULIB_IMPL_PRETTYDOC` once before including `mulib-prettydoc.h`; use `MULIB_IMPL_PRETTYEMIT` for the four output targets and `MULIB_IMPL_PRETTYSEXPR` for language-neutral S-expression input. If compiling those implementations in a single translation unit, define all three macros once and include only `mulib-prettysexpr.h`.

Unlike MuLIB's glyph-oriented `mulib-layout.h`, this kernel operates on *code structure*: `Text`, `SoftLine`, `HardLine`, `Concat`, `Group`, `Nest`, `Align`, `Choice`, `Annotate`, `Fill`, `Region` (local algorithm selection), and `ParagraphRegion` (local paragraph-strategy selection).

## Algorithms

- `MULIB_PRETTY_GREEDY` (mode 0): wrap on softline based on whether the following text fragment fits. This local greedy heuristic is **not** a complete Oppen-style streaming algorithm.
- `MULIB_PRETTY_FITS` (mode 1): Wadler-style fit test for the candidate flattened group; break otherwise.
- `MULIB_PRETTY_SEARCH` (mode 2): try up to `2^N` combinations of the first `N` encountered group choices, with `N <= 12`, and choose the lowest scored layout; fallback to fit tests when the group count exceeds the limit. This is **not** Bernardy's or PrettyExpressive's full optimality algorithm.
- `MULIB_PRETTY_WORD_GREEDY` and `MULIB_PRETTY_MIN_RAGGEDNESS` select greedy word wrapping or a quadratic dynamic-programming minimum-raggedness model for `Fill`. These are **not** a complete Knuth–Plass box/glue/penalty implementation; MuLIB already has one in `mulib-linebreak.h`.

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

Accepted document operators: `(text "string")`, `(fill "paragraph")`, `(soft)`, `(hard)`, `(cat A B)`, `(group A)`, `(nest COUNT A)`, `(align A)`, `(choice A B)`, `(annot "style" A)`, `(region "greedy|fits|search" A)`, `(paragraph "greedy|ragged" A)`. Strings accept escapes `\n`, `\r`, `\t`, `\\`, `\"` and `\xHH` (exactly two hexadecimal digits encoding one byte). Decoded text must be valid NUL-free UTF-8. The full input must be consumed; source is limited to 2 MiB, each decoded string to 1 MiB, parser depth to 128, and nodes to 200,000.

Semantic styles: `normal`, `keyword`, `identifier`, `number`, `operator`, `comment`, `string`, `math-ident`, `math-number`, `math-operator`.

All adapters (including languages other than On1x) can serialize the same format, or directly call the document-builder C API. The kernel does *not* parse arbitrary programming languages: each frontend maps its AST/CST/IR into the portable document algebra.

## Render targets

- `MULIB_PRETTY_PLAIN` returns the original formatted text.
- `MULIB_PRETTY_HTML_MATHML` emits standalone HTML5 source with `<pre><code>` and `<span class="tok-...">`; math token annotations turn into Presentation MathML `<mi>`, `<mn>`, or `<mo>` elements. This is token-level MathML, not a full symbolic-math AST layout engine.
- `MULIB_PRETTY_LATEX` emits a standalone LaTeX article with escaped monospaced source, bold keywords and italic comments. Unicode math and advanced LaTeX layouts require later work.
- `MULIB_PRETTY_TROFF` emits `.nf`/`.ft CW`/`.fi` preformatted source with `\` escapes and leading-dot/apostrophe protection. Styling beyond monospaced is not yet implemented.

Text is escaped for its target after layout. All targets use the same break decisions.

`mulib_pretty_layout` requires a zero-initialized output and returns 1/0. It is transaction-like on failure; release owned strings/pieces with `mulib_pretty_layout_free`. `mulib_pretty_emit` and `mulib_pretty_render` return `malloc`-owned UTF-8 strings, or NULL on failure. `mulib_prettydoc_free` owns every document node/text. Node handles are valid only in the owning document.

## Example native build

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/prettyprint-demo.c -o /tmp/pretty-demo
/tmp/pretty-demo
```

`make -f prettyprint.mk test` builds the `ppfmt` document renderer and runs
the native kernel, CLI and cross-TU bridge tests. Use `BUILD_DIR=...` to keep
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
| Header | [`include/mulib-prettydoc.h`](../include/mulib-prettydoc.h) |
| Implementation switch | `MULIB_IMPL_PRETTYDOC` |
| Repository-local includes | None |
| Complete example | [`examples/prettyprint-demo.c`](../examples/prettyprint-demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/prettyprint-demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
