# mulib-lzf

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

Raw LZF blocks and ZV-framed `.lzf` streams. Define `MULIB_IMPL_LZF`,
`MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER` in one translation unit.
[Complete example](../examples/modules/lzf.c).

`mulib_lzf_raw_encode` uses an 8192-entry bounded greedy matcher, 32-byte literal
groups, distances up to 8192 and matches up to 264 bytes. `raw_decode` implements
all literal, short-match and extended-match forms with checked overlapping copies.
An empty raw block is represented by zero bytes. No end marker or checksum exists;
the caller must know the exact compressed block size.

`mulib_lzf_encode` splits input into at most 65535-byte chunks. Each begins with
`ZV`, a type byte (0 stored or 1 compressed), a big-endian stored length and,
for type 1, a big-endian original length. Compressed chunks are used only when
smaller; incompressible data is stored. Empty input emits an explicit empty stored
chunk. `mulib_lzf_decode` accepts all chunks in the supplied range and verifies
their advertised decoded lengths. Unknown types return EUNSUPPORTED, missing
fields and invalid references return EFORMAT, budget overruns return ELIMIT.
Zero compressed input is rejected as missing framed input.

Calls append atomically, allow input/output aliasing, and share the common codec
ownership/error contracts. There is no checksum: structurally valid altered data
may decode without error. Both algorithms are linear with bounded match searches;
encoder workspace is 8192 size_t entries plus staging storage and chunk payloads.
Tests verify binary and highly repeated data, chunk boundaries, truncation,
allocation failures and independent liblzf raw payload compatibility when present.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lzf.h`](../include/mulib-lzf.h) |
| Implementation switch | `MULIB_IMPL_LZF` |
| Repository-local includes | [mulib-codec-core](mulib-codec-core.md) |
| Complete example | [`examples/modules/lzf.c`](../examples/modules/lzf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lzf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
