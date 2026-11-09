# mulib-lzma

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

LZMA1 and LZMA2 LZ/range coding, plus the 13-byte LZMA-alone wrapper. Define
`MULIB_IMPL_LZMA`, `MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER` in one
translation unit. [Complete example](../examples/modules/lzma.c).

`mulib_lzma_encode` emits deterministic LZMA-alone with lc=3, lp=0, pb=2, an
8 MiB dictionary, unknown-size header and EOS marker. The encoder uses a bounded
greedy hash matcher (matches up to 273 bytes), matched literals and adaptive range
coding. It does not use literal-only encoding or external codec backends.
`mulib_lzma_raw_encode` emits the same LZMA-302 payload without the file header.

`mulib_lzma_decode` accepts exactly one LZMA-alone stream with known or unknown
uncompressed size, all legal lc/lp/pb property bytes, normal literals, matched
literals, matches and all repeated-match forms. It checks dictionary/history
references, EOS and range finalization, output length and absence of trailing
bytes. An unknown-size stream requires EOS. Known-size streams may omit EOS.
There is no checksum in this format; use an integrity-protected wrapper if needed.

`mulib_lzma_raw_decode` takes a packed property byte and dictionary size and
decodes through EOS, reporting consumed bytes. This is intentionally a prefix
API for lzip; the count and output are unchanged on failure. Dictionary values
below 4096 are normalized to 4096. All history is actual decoded output, never a
preallocated advertised dictionary. Model storage is 16-bit probabilities:
1846 + (768 << (lc+lp)) entries, at most about 6 MiB for the largest legal LZMA1
context. Ordinary LZMA-302 uses roughly 16 KiB.

`mulib_lzma2_encode` emits independent 32 KiB compressed chunks with explicit
dictionary/model/property resets, falling back to stored chunks when smaller.
`mulib_lzma2_decode` accepts all standard stored/compressed/reset control forms,
retains models/history between chunks when requested, validates lc+lp<=4, and
checks each packed/unpacked length and final zero control byte. Its dictionary
argument is the decoded dictionary size, not an XZ property byte. It is also a
prefix API and reports consumed bytes, including the terminator. An empty LZMA2
payload is a single zero control byte; zero input bytes are malformed.

All functions follow the common atomic append/aliasing/limit contracts. Complexity
is linear with bounded bit-tree work and encoder match length; decoder model
initialization is proportional to the chosen context count. Tests include liblzma
LZMA-alone/XZ output, reset-heavy locally encoded streams, mutation and allocation
failure. LZMA-alone does not accept concatenated streams; lzip/XZ do.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lzma.h`](../include/mulib-lzma.h) |
| Implementation switch | `MULIB_IMPL_LZMA` |
| Repository-local includes | [mulib-codec-core](mulib-codec-core.md) |
| Complete example | [`examples/modules/lzma.c`](../examples/modules/lzma.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lzma.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
