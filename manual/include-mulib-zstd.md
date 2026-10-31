# include-mulib-zstd

## NAME AND SYNOPSIS

Dependency-free standard Zstandard frame encoder/decoder for raw and RLE blocks.

```c
#include "include-mulib-zstd.h"
```

Static-inline dependency using `include-mulib-container-core.h`; no switch or
external compression library. Example: `examples/modules/zstd-raw.c`.

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
