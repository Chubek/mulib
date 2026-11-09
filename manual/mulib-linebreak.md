# mulib-linebreak

[Manuals](README.md) · [Library index](index.md)

Seven line-breaking strategies for strict UTF-8 text, with configurable additive
advances and owned byte-range results. Unicode determines **legal break
opportunities**; paragraph optimization determines **which opportunities to use**.
These are separate concerns: `UNICODE` combines ICU opportunities with greedy
selection, while the other six strategies use the profiles below.

## Integration and lifecycle

Define `MULIB_IMPL_LINEBREAK`, `MULIB_IMPL_UNIBREAK`, `MULIB_IMPL_UNIPROC`,
`MULIB_IMPL_DYNLOAD`, and `MULIB_IMPL_UTF8` in exactly one translation unit before
including `mulib-linebreak.h`. No dependency switch is defined implicitly. ISO
C11 and C++17 declarations/implementation are supported. On Linux link `-lm -ldl`.
Only `UNICODE` needs a runtime ICU installation; other algorithms work without it.

Initialize options with `mulib_linebreak_options_init(&options, width)`, then call
`mulib_linebreak(text, byte_length, algorithm, &options, &result)`. `result` must
initially be zero-initialized, or be a previous successful result. Success frees
and replaces its old lines. Every failure leaves the complete old result intact.
`mulib_linebreak_result_free` frees and zeros it; NULL and repeated calls are safe.
Options initialization overwrites the options, does not allocate, and accepts NULL.

Text, options, callback context, locale and the optional ICU context are borrowed
only during the call. No source pointer is retained: results contain byte offsets,
and remain usable with an unchanged copy of the input. Do not mutate these inputs
or the output from callbacks. Independent calls are reentrant; callers serialize
calls sharing an ICU context as required by `mulib-uniproc`.

## Strategies

| Algorithm | Selection and objective |
| --- | --- |
| `GREEDY` | Take the furthest fitting word opportunity, proceeding left to right. |
| `CHARACTER` | Greedy wrapping at Unicode **scalar** boundaries. Never splits a UTF-8 sequence or CRLF pair, but may split a combining sequence or emoji grapheme. |
| `MINIMUM_RAGGEDNESS` | Global shortest-path dynamic programming minimizing the sum of squared unused natural width. A fitting final line has zero cost. |
| `BALANCED` | Determine the greedy line count, then globally minimize squared deviations from total paragraph natural width divided by that count, including the final line. The target includes internal spaces even when a chosen break later discards them. |
| `LOOKAHEAD` | For each first-line candidate, greedily fill a second line; choose the pair with the least squared raggedness. A fitting final line is free; ties prefer the later first break. Result cost sums the selected lines' individual raggedness, not overlapping pair scores. |
| `KNUTH_PLASS` | Global badness/demerit optimization with stretch/shrink ratios, four fitness classes, discretionary-hyphen penalties and consecutive-flagged-break demerits. See below. |
| `UNICODE` | Use `mulib-unibreak` ICU line opportunities (UAX #14 with the installed ICU version's data and locale tailoring), then greedy width selection. No ASCII-only fallback or emergency character splits. |

The non-Unicode word profile permits breaks after runs of ASCII SP/TAB, literal
ASCII hyphen, U+00AD SOFT HYPHEN and U+200B ZERO WIDTH SPACE. It is deliberately
**not** a substitute for UAX #14 punctuation, script, dictionary or no-break rules.
In particular, use `UNICODE` for CJK, word joiner constraints, combining sequences,
emoji and language-sensitive segmentation. Set `options.unicode` to a borrowed
`mulib_uniproc_open` context; NULL locale selects ICU root, not the process locale.
The installed ICU determines Unicode version and dictionary coverage; the header
does not embed a pinned Unicode property database.

All algorithms honor LF, CR, CRLF, VT, FF, NEL, LINE SEPARATOR and PARAGRAPH
SEPARATOR as mandatory breaks, optimizing each resulting paragraph independently.
CRLF is consumed as one separator. Consecutive separators produce empty lines;
a terminal separator does not add a phantom following line. Empty input, including
NULL with zero length, succeeds with zero lines (UNICODE still requires a backend).
Embedded NUL is a normal scalar, not a terminator.

## Measurement, whitespace and result ranges

Advances are additive: by default each scalar advances one unit and TAB advances
four fixed units, **not** to the next tab stop. Set `measure(context, scalar)` to
provide proportional or zero-width advances. It is called once per visible input
scalar and must return a finite value in `[0, 1e12]`. Hard separators, soft hyphen,
ZWSP and U+2060 WORD JOINER always have zero advance and bypass the callback.
Default combining marks are not automatically zero width. This is not a terminal
`wcwidth` implementation, text shaper, bidi renderer, or context-dependent kerning
model; precompute suitable scalar advances if integrating with shaping.

Each line contains:

- `start`, `end`: the display byte range `[start,end)`. Leading whitespace is
  preserved; SP/TAB immediately before the selected break is discarded.
- `next`: the consumed endpoint, including discarded spaces, discretionary
  markers or mandatory separators. The next line starts here. The final `next`
  equals input length; source bytes are neither duplicated nor lost.
- `width`: natural advance, excluding discarded spaces and mandatory separators.
- `adjustment`: Knuth-Plass glue adjustment ratio, or zero for other strategies.
- `flags`: `HARD` for a mandatory separator, `HYPHEN` to append a visible hyphen,
  and `OVERFULL` when natural advance exceeds the requested width.

A selected soft hyphen is excluded from the display range, consumes its original
UTF-8 bytes, and adds `hyphen_width` to the advance. Render an appended hyphen when
`HYPHEN` is set. Unselected soft hyphens, ZWSP and word joiners inside a range
remain invisible; the library returns source ranges, not sanitized strings.
No dictionary hyphenation or automatic insertion is performed; applications can
insert explicit soft hyphens using their own language/hyphenation model.

Except for Knuth-Plass, an indivisible segment wider than the limit is emitted
overfull, never silently split. Global raggedness and balancing permit such an
edge only between adjacent opportunities. Overflow costs squared error, including
on the last line. `OVERFULL` may also describe a Knuth-Plass line that legitimately
fits after shrinking; `width` is always natural, not post-justification width.

## Knuth-Plass profile

Internal ASCII SP/TAB provides glue. Each glue's stretch and shrink capacity is
its measured natural advance times `space_stretch` (default .5) and `space_shrink`
(default 1/3). Discarded line-end glue provides no capacity. For target `W`, natural
advance `w` and total relevant capacity `c`, the ratio is `(W-w)/c`. Exact-width
lines have ratio zero. Underfull final lines use free infinitely stretchable end
fill, represented by ratio zero; overfull final lines must still shrink feasibly.

Only ratios in `[-1,tolerance]` are admitted; default tolerance is 3. Nonzero
adjustment without any relevant capacity is infeasible. Badness is
`min(10000, 100*abs(ratio)^3)`. Fitness classes are tight (`r < -.5`), decent
(`r <= .5`), loose (`r <= 1`) and very loose. Dynamic programming retains a
separate best path for each class at each breakpoint. Each line contributes
`(line_penalty + badness)^2`, with defaults:

- line penalty 10;
- selected soft-hyphen penalty squared: `50^2`;
- adjacent classes differing by more than one: fitness demerit 100;
- consecutive selected soft hyphens: flagged demerit 100.

Initial fitness is decent. A final line's fitness participates in transitions.
`result.cost` sums paragraph demerits; returned ratios tell a renderer how much
to expand/shrink internal glue. No actual justification is performed.

This is a text/glue Knuth-Plass implementation, **not a full TeX paragraph engine**:
there is no arbitrary box/glue/penalty stream, negative/forced user penalties,
variable line widths, looseness, automatic hyphenation, emergency stretch or TeX
multi-pass behavior. A paragraph without a feasible path returns `INFEASIBLE`,
preserving the old result; callers may explicitly retry with different options
or another strategy.

## Returns, limits and complexity

`OK` (0), `INVALID` (-1), `MEMORY` (-2), `LIMIT` (-3), `BACKEND` (-4), and
`INFEASIBLE` (-5) distinguish success, invalid options/UTF-8/callback advance,
allocation failure, resource exhaustion, unavailable/failed ICU segmentation,
and no feasible justified path. ICU's existing boundary API does not distinguish
backend allocation failure from other backend errors, so those map to `BACKEND`.

- Input cap: 1 MiB, checked before reading input. All lengths exclude a terminator.
- Width: finite `[1e-6,1e12]`; hyphen width `[0,1e12]`.
- Stretch multiplier `[0,1000]`, shrink `[0,1]`, tolerance `[0,1000]`.
- Line/hyphen penalties `[0,1e6]`; fitness/flagged demerits `[0,1e12]`.
- All numeric options must be finite and nonnegative, even when unused.
- `max_work` defaults to 10000000 and must be positive. Each evaluated candidate
  edge, including reconstruction, consumes one unit. DP state counts must also
  fit this budget. Exhaustion is transactional, not a partial layout.
- Temporary arrays use checked allocation sizes; allocation failures are
  transactional. Scalar/prefix, break and output storage is O(input bytes).

For `B` opportunities and `L` greedy lines per hard-delimited paragraph, ordinary
greedy selection is O(B) apart from input decoding and backend segmentation;
nonmonotone discretionary widths can increase the worst case to O(B²).
Raggedness and Knuth-Plass use O(B²) work and O(B) optimizer storage.
Balancing uses O(L B²) work and O(L B) optimizer storage. Two-line lookahead has
an O(B³) conservative worst-case work bound. ICU segmentation's own work is not charged to
`max_work`, but its input is subject to the same byte cap. Scores and advances use
ordinary double-precision arithmetic; exact mathematical ties have no universal
typographic significance.

See the complete [example](../examples/modules/linebreak.c),
`tests/test_linebreak.c` (including exhaustive small-paragraph optimality oracles)
and `tests/test_linebreak_alloc.c` (allocation-failure preservation/leak checks).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-linebreak.h`](../include/mulib-linebreak.h) |
| Implementation switch | `MULIB_IMPL_LINEBREAK` |
| Repository-local includes | [mulib-unibreak](mulib-unibreak.md) |
| Complete example | [`examples/modules/linebreak.c`](../examples/modules/linebreak.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/linebreak.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
