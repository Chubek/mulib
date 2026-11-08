# mulib-unibreak

[Manuals](README.md) · [Library index](index.md)

Unicode line-break opportunities, extended grapheme boundaries, word and
sentence segmentation through ICU BreakIterator. This is a segmentation API;
it does not choose line widths, measure fonts or insert hyphenation glyphs.

## Integration

Define `MULIB_IMPL_UNIBREAK`, `MULIB_IMPL_UNIPROC`, `MULIB_IMPL_DYNLOAD`, and
`MULIB_IMPL_UTF8` before including `mulib-unibreak.h`. Link `-ldl` where needed.
Use an open Unicode context. Example: `examples/modules/unibreak.c`.

```c
mulib_unibreak_boundary_t *boundaries;
size_t count;
if (!mulib_unibreak_find(context, MULIB_UNIBREAK_LINE, "en",
                         text, byte_length, &boundaries, &count)) {
    /* boundaries[i].offset indexes the original UTF-8 string. */
    free(boundaries);
}
```

## Contract

Kinds are `GRAPHEME`, `WORD`, `LINE`, and `SENTENCE`. Input is strict,
length-delimited UTF-8; embedded NULs are supported. NULL input requires zero
length. NULL locale selects ICU's root locale. Locale tailoring and dictionary
segmentation (e.g. Thai) depend on the runtime's ICU data installation.

The returned array is malloc-owned and contains the initial byte offset zero,
all boundaries in ascending order, and the end offset. Empty input yields one
boundary at zero. Offsets never refer to the middle of a UTF-8 codepoint.
`status` is the ICU rule status at that boundary. Line status >=100 denotes a
mandatory break; lower statuses denote soft opportunities. The end of input is
always a boundary, but is not necessarily a mandatory-break rule status.
Word statuses classify the preceding segment (ICU's NONE, NUMBER, LETTER,
KANA and IDEO ranges); punctuation segments are not filtered out.

Return 0 on success, -1 on invalid arguments/UTF-8, missing backend symbols,
native errors or allocation failure. Outputs are unchanged on failure.
Temporary native iterators are closed before returning; the result is independent
of the input and Unicode context. Input length is limited to `INT32_MAX` bytes;
allocation products are checked. Memory is linear in input length.

## Scope and verification

Unicode UAX #14 and UAX #29 behavior follows installed ICU, including emoji ZWJ
graphemes and locale tailoring. There is no heuristic fallback if ICU is absent.
Tests in `tests/test_unicode_text.c` cover combining marks, ZWJ sequences,
CRLF mandatory breaks, empty input and invalid encodings. `tools/check_text.py`
can require backend availability and run ASan/UBSan.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-unibreak.h`](../include/mulib-unibreak.h) |
| Implementation switch | `MULIB_IMPL_UNIBREAK` |
| Repository-local includes | [mulib-uniproc](mulib-uniproc.md) |
| Complete example | [`examples/modules/unibreak.c`](../examples/modules/unibreak.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/unibreak.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
