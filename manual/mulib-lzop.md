# mulib-lzop

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

Lzop containers with dependency-free LZO1X compression/decompression. Define
`MULIB_IMPL_LZOP`, `MULIB_IMPL_LZO`, `MULIB_IMPL_ADLER32`, `MULIB_IMPL_CRC32`,
`MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER`. No liblzo dependency.
[Complete example](../examples/modules/lzop.c).

`mulib_lzop_encode` emits deterministic v1.03 headers, method 1, level 5, Unix
origin, fixed mode and zero timestamps, no filename, CRC-protected header and
data/compressed CRCs. Input is divided into 256 KiB blocks, using LZO1X when
smaller or stored blocks otherwise. A zero-sized block terminates the stream.
Empty input produces a header and terminator.

`mulib_lzop_decode` accepts concatenated streams with modern headers (version
at least 0x0940, required extraction version at most 0x1040), methods 1/2/3, stored
blocks and all original LZO1X instruction forms. It verifies all selected Adler32
and CRC32 data/compressed checksums, Adler32 or CRC32 header checksums and checked
extra-header fields. Filename and ordinary mode/time/origin metadata are ignored,
not used as filesystem paths. Header extensions for filters, multipart and GMT
offsets, reserved flags, unsupported methods/versions return EUNSUPPORTED. LZO-RLE
versioned raw bytecode is not part of this original LZO1X profile.

Unpacked block size is bounded against the aggregate limit before decoding;
stored size must be positive and no greater than unpacked size. Every block must
decode to exactly its declared length. Truncation, invalid bytecode/references,
bad terminators and trailing garbage are rejected. CRC/Adler failures produce
ECHECKSUM. See common contracts for failure-atomic appending and aliasing.
Workspace follows [LZO](mulib-lzo.md). Tests use independent liblzo2 payloads and
independently built/verified lzop frames, including compressed and stored blocks.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-lzop.h`](../include/mulib-lzop.h) |
| Implementation switch | `MULIB_IMPL_LZOP` |
| Repository-local includes | [mulib-adler32](mulib-adler32.md), [mulib-crc32](mulib-crc32.md), [mulib-lzo](mulib-lzo.md) |
| Complete example | [`examples/modules/lzop.c`](../examples/modules/lzop.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lzop.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
