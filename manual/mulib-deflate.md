# MuLIB DEFLATE: bounded raw compression

[Manuals](README.md) · [Library index](index.md)

## Integration and profile

Include `mulib-deflate.h`. Define `MULIB_IMPL_DEFLATE` and
`MULIB_IMPL_BUFFER` in exactly one translation unit. ISO C11, no external
libraries or native compression services. [Complete example](../examples/modules/deflate.c).

The wire format is **RFC 1951 raw DEFLATE**, without zlib/gzip headers, dictionaries,
checksums or concatenated streams. Decoding implements stored, fixed-Huffman and
dynamic-Huffman blocks, 32 KiB backward references, overlapping matches, empty
streams and multiple blocks. Malformed or oversubscribed Huffman trees, illegal
symbols, missing end codes, invalid distances, truncated blocks and trailing bytes
are rejected. Unused final-byte padding bits are ignored as allowed by the format.

Encoding produces one final fixed-Huffman block. A 4096-slot hash table remembers
one previous position per hash; greedy matches are limited to 258 bytes and a
32768-byte distance. Output is deterministic, but compression ratio is not promised;
incompressible input can expand. There is no compression-level setting.

## API and ownership

`mulib_deflate_encode(data, size, &out)` and
`mulib_deflate_decode(data, size, limit, &out)` append bytes to an initialized
`mulib_buffer_t`. Zero is success; negative `EINVAL`, `EFORMAT`, `ELIMIT`, and
`ENOMEM` distinguish bad arguments, malformed streams, output-limit exhaustion
and allocation failure. All failures preserve output length and contents.

Inputs are borrowed only during a call and may alias output. `(NULL, 0)` encodes
an empty stream; empty compressed input is malformed. The decoder's mandatory
limit applies to newly decoded bytes, not output's existing prefix. Neither API
adds a terminator. Call `mulib_buffer_free` on owned output. Scratch memory is
released on every path; there is no mutable global state.

The decoder buffers its result before appending, using O(decoded size) memory.
The encoder uses O(encoded size) scratch plus 4096 `size_t` entries. Matching work
per input position is bounded by 258 comparisons. Integer overflow and buffer
allocation failures are checked by the buffer layer.

## Verification

`tests/test_ebook_formats.c` covers boundaries, overlap, malformed/truncated input,
limits and roundtrips. `tests/test_ebook_alloc.c` injects allocation failures.
The shared `tests/fuzz_headers.c` harness also exercises bounded decoding.
`tools/check_ebook_formats.py` tests interoperability against Python zlib with all
three block types, multiple blocks, several strategies and independently decoded
MuLIB output.

Reference: [RFC 1951](https://www.rfc-editor.org/rfc/rfc1951).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-deflate.h`](../include/mulib-deflate.h) |
| Implementation switch | `MULIB_IMPL_DEFLATE` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/deflate.c`](../examples/modules/deflate.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/deflate.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
