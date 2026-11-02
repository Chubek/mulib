# mulib-bidi

Unicode bidirectional paragraph resolution through ICU's UAX #9 implementation,
including explicit embeddings/overrides, isolates, weak/neutral resolution,
paired brackets and implicit levels as supported by the installed ICU version.

## Integration

Define `MULIB_IMPL_BIDI`, `MULIB_IMPL_UNIPROC`, `MULIB_IMPL_DYNLOAD`, and
`MULIB_IMPL_UTF8` before including `mulib-bidi.h`. Link `-ldl` where needed.
Example: `examples/modules/bidi.c`.

Initialize `mulib_bidi_result_t result = {0}` and call
`mulib_bidi_resolve(context, text, byte_length, base, &result)`.
`base` is 0 (LTR), 1 (RTL), -1 (first strong, default LTR), or -2 (first strong,
default RTL). A successful result contains the resolved paragraph level and
logical UTF-8 byte ranges in **visual run order**. Each run includes its
resolved embedding level; odd levels mean RTL, even levels mean LTR.

Do not reverse UTF-8 bytes or individual combining marks. Pass the logical
substring to `mulib_fontshape_shape` with the run's direction; HarfBuzz produces
the glyph order within that run. Cluster indices are relative to the substring,
so add the run's `start` when mapping back to the original input. This API does
not remove bidi controls from source text or emit a mirrored display string;
mirroring belongs to the shaping/rendering stage.

## Ownership and failures

Input is borrowed only during the call and must be strict UTF-8. Embedded NULs
are valid; NULL input requires zero length. Output must be empty (zero-init or
previously destroyed). On success (0), the result owns its runs; release them
with `mulib_bidi_destroy`, which zeroes the result and accepts NULL. Failure
(-1) leaves output unchanged. Invalid arguments/UTF-8, unavailable symbols,
native failures and OOM all fail. The Unicode context is borrowed and can be
closed after the call; the runs are independent copies.

Lengths are limited to `INT32_MAX` input bytes and allocation arithmetic is
checked. Empty input returns zero runs and the resolved default paragraph level.
For wrapped paragraphs use `mulib_bidi_paragraph_open`, then
`mulib_bidi_paragraph_line(start, length, &result)` for each scalar-aligned byte
range. The paragraph owns copied UTF-16 text and byte mappings and retains native
resolution state; the Unicode context must outlive it. Line extraction applies
ICU's L1/L2 handling without losing enclosing embeddings, isolates or paragraph
direction. Returned runs have **paragraph-absolute** byte offsets. Zero-length
ranges succeed with no runs; a line cannot span paragraph separators. For text
containing several paragraphs, ranges within each use that paragraph's base.
Close with `mulib_bidi_paragraph_close`. Failures leave output unchanged.

`mulib_pglayout_compute_shaped` uses this pipeline and retains shaped glyphs for
final lines. The older standalone `font_measure` adapter still resolves isolated
substrings and is retained for simple/custom integrations.

## Verification

`tests/test_unicode_text.c` checks mixed Hebrew/Latin runs, numeric levels in an
RTL paragraph, byte offsets, empty input and malformed UTF-8. Optional native
ICU comparisons run through `python3 tools/check_text.py`, including line
extraction for every subrange of an isolate/bracket sample.
See [text hardening](text-hardening.md) for official corpus results and the
installed ICU backend's known normative deviations; native parity is not a
promise of zero failures against every UAX #9 case.
