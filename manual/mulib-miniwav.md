# mulib-miniwav

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-miniwav.h`](../include/mulib-miniwav.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-miniwav.h
 * @brief RIFF/WAVE PCM and IEEE float memory encoder/decoder.
 */
#ifndef MULIB_MINIWAV_H
#define MULIB_MINIWAV_H
#include "mulib-audio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** Decode little-endian RIFF PCM8/16/24/32 or IEEE float32/64, including
 * WAVE_FORMAT_EXTENSIBLE with matching valid bits. Output must be empty.
 * Unknown chunks are skipped; RF64, RIFX and compressed WAV are unsupported.
 * Failures leave output unchanged. SIMD accelerates integer normalization. */
mulib_audio_status_t mulib_miniwav_decode (const void *, size_t,
    const mulib_audio_options_t *, mulib_audio_pcm_t *);
/** Encode PCM8/16/24/32 or float32 (floating=1, bits=32).
 * Integer samples are rounded to nearest, clipped to [-1,1). Output must be
 * empty; failure leaves it unchanged. Empty PCM produces a valid empty WAV. */
mulib_audio_status_t mulib_miniwav_encode (const mulib_audio_pcm_t *, unsigned bits,
    int floating, const mulib_audio_options_t *, mulib_audio_bytes_t *);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-miniwav.h`](../include/mulib-miniwav.h) |
| Implementation switch | `MULIB_IMPL_MINIWAV` |
| Repository-local includes | [mulib-audio](mulib-audio.md) |
| Complete example | [`examples/modules/miniwav.c`](../examples/modules/miniwav.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/miniwav.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
