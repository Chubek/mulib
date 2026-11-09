# Dependency-free compression codecs

[Manuals](README.md) · [Library index](index.md)

MuLIB implements memory-to-memory gzip, bzip2, Zstandard, XZ, LZMA-alone,
lzip, LZF and lzop compression/decompression and ZIP32 archive creation/reading.
These are implementations, not subprocess adapters or dynamically loaded codecs.
There are no third-party runtime dependencies. Encoders use deliberately simple,
bounded algorithms, not the reference tools' highest-ratio optimizers. A codec
need not emit every coding mode its decoder accepts.

## Integration and ownership

Each module documents its implementation switches. Define **all** switches in
one translation unit before including the module. Do not define them in every
client. The application owns implementation placement. C11 and C++17 declaration
clients are supported; no filesystem, locale, environment or global mutable state
is used. Independent calls can run concurrently; synchronize shared outputs.

New codec functions take an initialized `mulib_buffer_t` and append atomically:
success preserves the old prefix and appends the result; every failure preserves
the output's bytes, length, pointer and capacity. Inputs may alias the old output
and are borrowed only during the call. `(NULL,0)` is valid encoder input. NULL
with a nonzero length is an argument error. A complete framed stream is required
for decoding, even when its decoded content is empty. Free owned output with
`mulib_buffer_free`; never shallow-copy live buffers.

The decoder's `limit` bounds the **aggregate newly decoded bytes**, not the
initial output prefix. Concatenated members/frames share this budget. No decoder
allocates an advertised dictionary: its history is validated against actual output
and any supplied dictionary. Temporary decoded output is bounded by `limit`.
Final append can temporarily coexist with the old output and the temporary result.
Metadata/workspace allocations are described in the module manuals. `SIZE_MAX`
is a valid but generally unsafe limit for untrusted data; use an application budget.

The older ZIP API uses its own error enum and logical-atomic writer contract.
The older Zstandard `raw_*` helpers use CT blobs, replacement rather than append,
CT allocator hooks, and a restricted decoding profile. Their API is unchanged.

## Results

New codec APIs return `MULIB_CODEC_OK` (zero), or:

| Result | Meaning |
| --- | --- |
| `MULIB_CODEC_EINVAL` | Invalid pointer, buffer state or parameter |
| `MULIB_CODEC_EFORMAT` | Truncation, bad framing, illegal reference/entropy code or size mismatch |
| `MULIB_CODEC_ELIMIT` | Output budget or representable size exceeded |
| `MULIB_CODEC_ENOMEM` | Allocation failure |
| `MULIB_CODEC_EUNSUPPORTED` | Unsupported version, filter, codec extension or missing/wrong dictionary |
| `MULIB_CODEC_ECHECKSUM` | Header, block or content integrity check mismatch |

Precedence between malformed input, an exceeded budget and a checksum failure is
not guaranteed. Checksums detect accidental corruption; they are not authentication.
LZF and raw LZO/LZMA have no checksum and can accept modified but structurally valid
data. Every decoder rejects unconsumed garbage unless a documented prefix API
explicitly reports `consumed`. Such counts also remain unchanged on failure.

## Verification and provenance

`tests/test_compression_codecs.c` checks binary/empty/boundary data, actual size
reduction, prefixes, overlap, truncation, mutation, gzip optional fields, ZIP
budgets and checksum vectors. `tests/test_compression_alloc.c` fails every reached
allocation in each encoder and decoder, checks atomicity and counts live blocks.
`tools/check_compression.py` cross-checks both directions with Python's zlib/bz2/
liblzma, the optional `zstd` command and optional system liblzf/liblzo2 test oracles.
Those libraries are **test-only**, never linked into MuLIB. CMake registers the
tests and `tools/check.py` runs the interoperability check with the focused suite.
The optional `mulib_fuzz_compression` Clang target exercises bounded hostile input.
These tests are evidence of compatibility, not a security audit or a claim that
every historical extension of each format is supported.

Implementations were written for MuLIB from format descriptions; no third-party
codec source is vendored. References are RFC 1951/1952, RFC 8878, the public-domain
[XZ format specification](https://tukaani.org/xz/xz-file-format.txt), the
[lzip format/algorithm manual](https://www.nongnu.org/lzip/manual/lzip_manual.html),
the bzip2 BWT/MTF/RLE/Huffman format, LZF ZV framing, and the Linux kernel's
[LZO instruction description](https://www.kernel.org/doc/html/latest/staging/lzo.html).
