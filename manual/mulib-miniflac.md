# mulib-miniflac

[Manuals](README.md) · [Library index](index.md)

Original native FLAC implementation, without libFLAC or other external codecs.
Define `MULIB_IMPL_MINIFLAC`, `MULIB_IMPL_AUDIO`, `MULIB_IMPL_XSIMD`; link `-lm`
on Unix. Example: `examples/modules/miniflac.c`.

## Decoder

`decode` supports native `fLaC` streams, 4..24-bit samples, 1..8 channels,
constant, verbatim, fixed-predictor orders 0..4 and LPC orders 1..32, wasted bits,
Rice/Rice2 residual partitions and escape coding. Independent channels and all
three stereo decorrelation assignments are supported. Frame header CRC8 and
frame CRC16, stream numbering, sample rate/precision, metadata lengths and sample
ranges are checked. Nonzero STREAMINFO total samples must match decoded samples.
Unknown metadata is skipped. STREAMINFO MD5 is **not** verified. Ogg FLAC and
sample precision above 24 bits are unsupported. No seeking or streaming API.

The decoded float PCM is exact for supported integer precision after normalized
scaling. Frame scratch uses up to approximately 12 bytes per sample/channel,
plus accumulated output. Frame sizes are bounded by STREAMINFO (maximum 65535).
Decoded frame count is bounded by the audio options. Unary residuals are bounded
by input length and a 32-bit quotient cap; impossible reconstructed samples fail.

## Encoder

`encode(pcm,bits,options,&bytes)` writes 16- or 24-bit native FLAC with independent
channels and 4096-frame blocks. Final blocks may be shorter. It currently uses
**verbatim** subframes: valid lossless FLAC, without predictive compression.
Integer PCM is rounded/clipped first; losslessness refers to that integer PCM.
MD5 is marked unavailable with all-zero STREAMINFO MD5, permitted by FLAC.
Both frame CRCs are written. Empty streams are supported.

Output objects must be zero-initialized and released with the audio free APIs.
SIMD accelerates normalization/quantization scaling. Failures leave output
unchanged. Optional external-tool tests verify files with `flac -t` and decode
reference-encoded predictor streams; tools are test oracles, not dependencies.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-miniflac.h`](../include/mulib-miniflac.h) |
| Implementation switch | `MULIB_IMPL_MINIFLAC` |
| Repository-local includes | [mulib-audio](mulib-audio.md) |
| Complete example | [`examples/modules/miniflac.c`](../examples/modules/miniflac.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/miniflac.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
