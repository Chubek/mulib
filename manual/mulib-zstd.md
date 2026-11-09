# mulib-zstd

[Manuals](README.md) · [Library index](index.md)

## NAME AND SYNOPSIS

Dependency-free Zstandard compression/decompression, including LZ sequences,
FSE and Huffman entropy decoding, plus the original restricted raw/RLE helpers.

```c
#include "mulib-zstd.h"
```

For the full API define `MULIB_IMPL_ZSTD`, `MULIB_IMPL_CODEC_CORE` and
`MULIB_IMPL_BUFFER` in one translation unit. No external libraries.
[Complete example](../examples/modules/zstd.c).

## FULL CODEC

`mulib_zstd_encode(data,size,out)` appends a deterministic single-segment frame
with a content size and XXH64 content checksum. It performs bounded greedy LZ
matching, emitting compressed blocks with raw literals, one sequence per block
and RLE-coded sequence tables, or raw blocks when compression would be larger.
This is a general LZ compressor, not the raw/RLE-only helper below. It does not
attempt reference zstd's higher-ratio multisequence/FSE/Huffman optimization.
The match hash has 16384 entries; blocks are at most 128 KiB.

`mulib_zstd_decode(data,size,limit,out)` accepts concatenated ordinary/skippable
frames, single-segment or explicit windows, known or unknown content size, and
optional checksums. It decodes raw/RLE/compressed blocks; raw/RLE/Huffman/treeless
literals (1 or 4 streams); FSE-compressed Huffman weights; predefined, RLE, FSE
and repeated sequence tables; all literals/match codes; and repeat-offset updates.
Entropy-table bounds, content/block sizes, dictionary/history references, exact
bitstream consumption, truncation and XXH64 are checked. Reserved bit 3 is rejected;
unused descriptor bit 4 is ignored as required by RFC 8878. Skippable payloads
are length-checked, not interpreted. Trailing garbage fails.

`mulib_zstd_decode_dict` additionally borrows a raw-content or trained dictionary.
Trained dictionaries supply Huffman/FSE tables, initial repeat offsets and history;
dictionary IDs are verified when present. Missing/wrong dictionaries produce
EUNSUPPORTED. A dictionary is never fetched or loaded implicitly. History can
reference its suffix and actual frame output, but never a previous frame.

The full functions use `mulib_buffer_t` and the [common codec contracts](compression-codecs.md):
atomic append, input/output aliasing, aggregate decoded-byte budget, common errors.
Workspace comprises bounded entropy tables, at most 128 KiB literals and staged
decoded output; advertised windows are never allocated. Decode is linear with
bounded bit operations, independent frames reset all entropy/history state.
Tests exercise system zstd levels 1/3/7/12/19 in both directions, not just local
roundtrips, plus mutation/truncation and injected allocation failures.

## RESTRICTED RAW HELPERS

The following older static-inline API uses `mulib-container-core.h` and needs no
implementation switch. Its CT blob API and restricted profile remain unchanged.
Example: `examples/modules/zstd-raw.c`.

## ENCODING PROFILE

`mulib_zstd_raw_encode(input,length,out,limit)` emits magic `28 b5 2f fd`, a
single-segment frame, eight-byte content size, no dictionary/checksum and blocks
at most 128 KiB. Runs of eight or more identical bytes use standard RLE blocks;
other spans use raw blocks. Empty input emits a valid final empty raw block.

Output is a standard Zstandard stream readable by system `zstd`. This profile
compresses zero-filled/tar padding effectively and preserves arbitrary binary
bytes. It does not implement entropy coding or a compression-level parameter.

## DECODING PROFILE

`mulib_zstd_raw_decode(input,length,out,max_output)` accepts one complete
single-segment raw/RLE frame, including legal 1/2/4/8-byte content-size encodings
(two-byte sizes include the standard +256 bias). Before allocating output, it
bounds the declared size and walks all block metadata to validate lengths,
input truncation, final block, exact content size and absence of trailing bytes.
Malformed or unsupported block layouts therefore fail without allocating the
advertised output. A second pass copies the validated payload; input must remain
immutable throughout the call.

Entropy-compressed blocks, non-single-segment windows, dictionaries and content
checksums return ENOTSUP. Reserved descriptor bits, reserved block types, invalid
lengths, concatenated/trailing frames and bad magic return EFORMAT. Skippable-frame
magic is not accepted. CRC/authentication is provided by the enclosing image format
or application, not this restricted Zstandard profile.

## OWNERSHIP AND LIMITS

Both functions require an initialized CT blob output and replace it atomically;
input can alias old output. Failure releases temporary storage and preserves out.
Input/output pointers must refer to accessible ranges. Decode allocation is bounded
by max_output even for malicious advertised sizes. Free outputs with CT blob free.

Use [container-packzstd](mulib-container-packzstd.md) for image validation and an
optional full-codec callback. Tests include empty/random/repeated data and
interoperability with the installed `zstd` command when available.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-zstd.h`](../include/mulib-zstd.h) |
| Implementation switch | `MULIB_IMPL_ZSTD` |
| Repository-local includes | [mulib-codec-core](mulib-codec-core.md), [mulib-container-core](mulib-container-core.md) |
| Complete example | [`examples/modules/zstd.c`](../examples/modules/zstd.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/zstd.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
