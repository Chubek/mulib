# OpenType ligature controls and clusters

[Manuals](README.md) · [Library index](index.md)

`include/mulib-ligature.h` operates on real HarfBuzz glyph output. It does not
replace character strings with Unicode presentation-form characters, which
would lose font-specific shaping and source correspondence.

Define `MULIB_IMPL_LIGATURE`, `MULIB_IMPL_FONTSHAPE`, `MULIB_IMPL_DYNLOAD`, and
`MULIB_IMPL_UTF8` before including the header in one TU. Link the platform dynamic
loader (`-ldl` on Linux). `examples/modules/ligature.c` runs without native fonts
using an illustrative cluster map; `tests/test_text_layout.c` verifies actual
OpenType ligature shaping and glyph counts with native backends.

## APIs

`mulib_ligature_shape(font,text,length,flags,options,&run)` has fontshape's output
and ownership contract: empty output, 0 success/-1 failure, unchanged on failure.
Flags select STANDARD (`liga`), CONTEXTUAL (`clig`), DISCRETIONARY (`dlig`) and
HISTORICAL (`hlig`). OR flags together; zero disables those four optional
features. Required script features (`rlig`, etc.) remain the shaper's defaults.
Unknown bits fail. Caller feature strings are appended after these defaults,
allowing explicit range overrides. Direction/script/language follow fontshape.

`mulib_ligature_clusters(text,length,&run,&clusters,&count)` produces an owned
visual-order map. Each record contains byte start/length, first glyph/count, and
Unicode scalar count. Input must be strict UTF-8 and clusters must be in bounds,
at scalar boundaries, and monotone ascending or descending (duplicates group
adjacent glyphs). Final logical cluster ends at length. Earlier source bytes
before the first logical cluster need not be represented (e.g. removed controls
or absolute-offset subruns). This is a source map, not proof that a GSUB ligature
was used: marks and emoji can also form multi-scalar clusters. A run with no
glyphs returns NULL/zero. All allocation products are overflow checked. Outputs
are unchanged on failure; free the array with `free`.

The text must match the shaping input, and ranges must be adapted if clusters
have been rebased. `mulib-glyph` uses this map when flattening its visual runs.

For interior ligature carets, `mulib_fontshape_carets` reads actual OpenType GDEF
positions. It returns owned signed positions in the font's configured scale and
the requested direction. Missing GDEF carets produce a successful empty array;
the API never synthesizes evenly spaced positions. Layout hit testing instead
uses cluster edges so it cannot split combining sequences.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ligature.h`](../include/mulib-ligature.h) |
| Implementation switch | `MULIB_IMPL_LIGATURE` |
| Repository-local includes | [mulib-fontshape](mulib-fontshape.md) |
| Complete example | [`examples/modules/ligature.c`](../examples/modules/ligature.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ligature.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
