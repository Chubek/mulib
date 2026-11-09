# mulib-codec-core

[Manuals](README.md) · [Codec contracts](compression-codecs.md)

Shared checked storage, wire integers, MSB-first bit I/O, CRC64/XZ and seed-zero
XXH64. Define `MULIB_IMPL_CODEC_CORE` and `MULIB_IMPL_BUFFER` before inclusion in
one translation unit. [Complete example](../examples/modules/codec-core.c).

`mulib_codec_buffer_valid` checks buffer shape, not whether an arbitrary address
is accessible. `reserve` checks the hard allocation bound, grows geometrically
without exceeding it, and leaves logical length unchanged. `append` copies
**nonaliasing** bytes within a total-length bound. `match` requires a positive
distance no greater than current output length, supports overlapping references,
and validates size arithmetic before allocation. These storage operations leave
the buffer unchanged on failure. The complete codecs stage temporary storage to
support aliasing and failure-atomic appends.

Endian loads/stores require accessible 4/8-byte ranges and accept unaligned
addresses. CRC64 and XXH64 require readable input (NULL only at zero length);
they return corruption checks, not authentication tags. CRC64 uses the XZ
reflected ECMA polynomial, all-ones initialization and final complement.

Zero-initialize bit readers/writers, then set reader data/size. `mulib_codec_get`
reads 0..32 MSB-first bits and returns Boolean success; a truncated read may advance
the reader but leaves its scalar output unchanged. Writer `put` returns a codec
result, may leave partial internal bytes on failure, and requires `flush` to pad
the final byte with zeros. Free `writer.bytes` even after failure. Bit I/O is a
low-level helper, not a transactional public framing parser. Checksums are linear;
match copying is linear; no globals or external libraries are used.

Tests: `tests/test_compression_codecs.c`, `tests/test_compression_alloc.c`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-codec-core.h`](../include/mulib-codec-core.h) |
| Implementation switch | `MULIB_IMPL_CODEC_CORE` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/codec-core.c`](../examples/modules/codec-core.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/codec-core.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
