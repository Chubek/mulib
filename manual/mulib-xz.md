# mulib-xz

[Manuals](README.md) · [Common codec contracts](compression-codecs.md)

XZ container parsing/writing with a dependency-free LZMA2 codec. Define
`MULIB_IMPL_XZ`, `MULIB_IMPL_LZMA`, `MULIB_IMPL_CODEC_CORE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_CRC32`, `MULIB_IMPL_AUTHCRYPTO` and `MULIB_IMPL_BCJ` before inclusion
in one translation unit. [Complete example](../examples/modules/xz.c).

`mulib_xz_encode` emits one deterministic stream with one LZMA2 block, an 8 MiB
dictionary, CRC64 data check, canonical index and matching footer. The LZMA2
encoder really compresses using LZ/range coding, falling back to stored chunks
only when smaller. Empty input produces a valid stream.

`mulib_xz_decode` handles multiple blocks, concatenated streams, four-byte-aligned
stream padding, optional packed/unpacked size fields, and checks None, CRC32,
CRC64 or SHA256. It checks header/block/index/footer CRCs, canonical bounded VLIs,
reserved flags, all padding bytes, each index record against the actual decoded
block, backward size and footer flags/magic. All LZMA2 chunk/reset modes and
dictionary property bytes 0..40 are supported without dictionary-sized allocation.

Filter chains can contain Delta and the classic BCJ filters x86, PowerPC, IA64,
ARM, Thumb and SPARC, followed by LZMA2. Start offsets/alignment and property
lengths are validated. Inverse filters are applied in reverse order before data
checksum verification. Newer ARM64/RISC-V and custom filters return EUNSUPPORTED;
unknown integrity checks likewise fail rather than being skipped.

Functions follow the common atomic append/aliasing/aggregate-limit contract.
Decoding work is linear in compressed/decoded data; index validation retains two
canonical VLI size values per block (at most 18 bytes per block) and checks them
exactly, rather than trusting a noncryptographic aggregate hash. Other workspace
is the shared LZMA model and bounded staged output. Errors distinguish format,
budget, allocation, unsupported features and integrity failure. Tests cover
independent liblzma compression, every supported filter and checksum, multiple
streams, corruption and injected allocation failures.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-xz.h`](../include/mulib-xz.h) |
| Implementation switch | `MULIB_IMPL_XZ` |
| Repository-local includes | [mulib-authcrypto](mulib-authcrypto.md), [mulib-bcj](mulib-bcj.md), [mulib-crc32](mulib-crc32.md), [mulib-lzma](mulib-lzma.md) |
| Complete example | [`examples/modules/xz.c`](../examples/modules/xz.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/xz.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
