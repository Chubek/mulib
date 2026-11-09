# mulib-lzip

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

Lzip version 1 with LZMA-302 EOS payloads. Define `MULIB_IMPL_LZIP`,
`MULIB_IMPL_LZMA`, `MULIB_IMPL_CRC32`, `MULIB_IMPL_CODEC_CORE` and
`MULIB_IMPL_BUFFER`. [Complete example](../examples/modules/lzip.c).

`mulib_lzip_encode` appends one deterministic member with an 8 MiB dictionary,
greedy LZ/range compression, and a trailer containing CRC32, 64-bit decoded size
and 64-bit total member size. Empty input is supported. It borrows input and
allows aliasing with output.

`mulib_lzip_decode` decodes all concatenated v1 members under one aggregate
decoded-byte limit, validates coded dictionary sizes from 4 KiB through 512 MiB,
requires a valid LZMA EOS marker, and independently checks all three trailer
fields. Fractional dictionary encodings are supported. Even when a trailer claims
a huge decoded size, allocation follows actual bounded output, not the claim.
Malformed header/dictionary/size fields give EFORMAT, unsupported versions give
EUNSUPPORTED, CRC mismatches give ECHECKSUM. All errors use the common enum.

This is strict memory decoding: trailing padding or text is rejected, unlike
some CLI tools' permissive trailing-data modes. Empty members in concatenated
input are accepted. No filesystem names, permissions or recovery operations are
implemented. Calls are failure-atomic. Workspace and complexity are those of the
shared LZMA-302 codec. Tests cross-check payloads against independent liblzma,
validate trailers independently and exercise concatenation and allocation failure.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lzip.h`](../include/mulib-lzip.h) |
| Implementation switch | `MULIB_IMPL_LZIP` |
| Repository-local includes | [mulib-crc32](mulib-crc32.md), [mulib-lzma](mulib-lzma.md) |
| Complete example | [`examples/modules/lzip.c`](../examples/modules/lzip.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lzip.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
