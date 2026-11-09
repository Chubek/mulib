# mulib-gzip

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

RFC 1952 gzip with a full RFC 1951 DEFLATE encoder/decoder, including dynamic,
fixed and stored blocks on input. Define `MULIB_IMPL_GZIP`, `MULIB_IMPL_DEFLATE`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER` in one
translation unit. [Complete example](../examples/modules/gzip.c).

`mulib_gzip_encode(data,size,out)` appends one deterministic member: DEFLATE,
no optional header fields, zero mtime and OS=255. Compression uses the existing
bounded greedy LZ77/fixed-Huffman DEFLATE compressor; it is not a store-only wrapper.
Empty input is a valid member. No compression-level or filename parameter exists.

`mulib_gzip_decode(data,size,limit,out)` decodes every concatenated member under
one aggregate output budget. It bounds and skips FEXTRA, consumes NUL-terminated
FNAME/FCOMMENT, accepts FTEXT, checks optional FHCRC (low 16 bits of IEEE CRC32),
and verifies every member's data CRC32 and ISIZE modulo 2^32. Truncated strings,
reserved flag bits, malformed DEFLATE, missing trailers and trailing garbage fail.
Compression methods other than 8 are unsupported. Header metadata is not returned
or used for filesystem operations. Empty compressed input is malformed.

Both APIs borrow input and append atomically to an initialized byte buffer;
aliasing is supported. Errors use `MULIB_CODEC_*`. Decoding is linear in decoded
size and parsed headers, with temporary output bounded by limit. Member handling
uses the shared `mulib_deflate_decode_prefix` API instead of scanning for trailer
signatures. Tests include independent Python gzip streams and hand-built optional
headers, header CRC corruption, truncations, concatenation and allocation failure.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-gzip.h`](../include/mulib-gzip.h) |
| Implementation switch | `MULIB_IMPL_GZIP` |
| Repository-local includes | [mulib-codec-core](mulib-codec-core.md), [mulib-crc32](mulib-crc32.md), [mulib-deflate](mulib-deflate.md) |
| Complete example | [`examples/modules/gzip.c`](../examples/modules/gzip.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gzip.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
