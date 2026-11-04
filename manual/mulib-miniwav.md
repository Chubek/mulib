# mulib-miniwav

Standalone RIFF/WAVE decoding and encoding. Define `MULIB_IMPL_MINIWAV`,
`MULIB_IMPL_AUDIO`, and `MULIB_IMPL_XSIMD`; include `mulib-miniwav.h` and link
the platform math library (`-lm` on Unix). Example: `examples/modules/miniwav.c`.

`decode(bytes,size,options,&pcm)` accepts little-endian RIFF/WAVE with one format
chunk and one data chunk, in either order. Unknown chunks and odd-byte padding
are checked and skipped. Supported input:

* Unsigned PCM8 and signed PCM16/24/32.
* IEEE float32/64, converted to finite float32 output.
* WAVE_FORMAT_EXTENSIBLE PCM/float GUIDs when valid bits equal container bits.

Rate, byte rate, block alignment, frame divisibility and chunk boundaries are
validated. RF64, RIFX, compressed codecs and differing valid/container widths
return `UNSUPPORTED`. Channel masks are not retained; channels keep file order.
Trailing bytes outside the RIFF container are ignored. Float support assumes
binary IEEE representations with four-byte float and eight-byte double.

`encode(pcm,bits,floating,options,&bytes)` writes PCM8/16/24/32 or float32 when
`floating=1,bits=32`. Integer samples are rounded half away from zero and clipped
to the representable range. Float PCM preserves finite samples. Empty PCM is
valid. Classic RIFF size is limited to 32 bits, including padding. Float output
uses format tag 3 and a 16-byte format chunk.

Both outputs must be empty/zero-initialized; release with `mulib_audio_*_free`.
Failures preserve output. Resource/SIMD options and file helpers are described
in [mulib-audio](mulib-audio.md). Integer normalization/scaling uses xsimd;
byte unpacking remains scalar.
