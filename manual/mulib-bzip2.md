# mulib-bzip2

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

Dependency-free bzip2 compression/decompression. Define `MULIB_IMPL_BZIP2`,
`MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER` in one translation unit.
[Complete example](../examples/modules/bzip2.c).

`mulib_bzip2_encode(data,size,out)` performs RLE1, cyclic Burrows-Wheeler sorting,
move-to-front, RUNA/RUNB coding and canonical Huffman coding. It emits BZh1 with
64,000-byte input blocks, two identical fixed 9-bit Huffman tables and block and
combined stream CRCs. Sorting uses stable counting passes over doubled suffix
classes: O(n log n) per block, bounded workspace independent of total file size.
The conservative Huffman strategy is interoperable, but not a maximum-ratio
replacement for bzip2's adaptive table optimizer. Empty streams are supported.

`mulib_bzip2_decode(data,size,limit,out)` accepts concatenated BZh1..BZh9 streams,
2..6 Huffman groups, selector MTF, changing code lengths, MTF/RLE2, inverse BWT and
RLE1. It checks tree bounds/oversubscription, selector counts, block limits,
origin pointers, end markers, zero alignment padding and both CRC layers. Block
storage is at most 900,000 bytes plus a 32-bit inverse-BWT index per block;
temporary decoded output is bounded by limit. Obsolete randomized blocks return
`MULIB_CODEC_EUNSUPPORTED`; they are not silently treated as ordinary blocks.
Other bad framing/entropy/size input returns EFORMAT and CRC failures ECHECKSUM.
Trailing bytes are rejected. Decode is linear in compressed and decoded size
apart from bounded canonical-code lookup (at most 20 bits per symbol).

Calls borrow input, allow input/output aliasing and append atomically. See the
common contract for errors and ownership. Independent Python bz2 streams exercise
all block sizes in ordinary use; allocation, mutation and truncation tests are in
the focused compression suites. This does not claim support for the historical
randomization extension.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bzip2.h`](../include/mulib-bzip2.h) |
| Implementation switch | `MULIB_IMPL_BZIP2` |
| Repository-local includes | [mulib-codec-core](mulib-codec-core.md) |
| Complete example | [`examples/modules/bzip2.c`](../examples/modules/bzip2.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bzip2.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
