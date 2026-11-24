# mulib-minimpeg

[Manuals](README.md) · [Library index](index.md)

Original MPEG-1 **Layer III (MP3)** encoding and profile-limited decoding, plus
optional **MPEG-1 Video** encoding and decoding. Audio and video have separate
entry points. The module does not handle MPEG audio Layers I/II or multiplexed
audio/video. For MP3 only, define `MULIB_IMPL_MINIMPEG`, `MULIB_IMPL_AUDIO` and
`MULIB_IMPL_XSIMD` and link `-lm` on Unix. No FFmpeg headers or libraries are
needed for MP3. See `examples/modules/minimpeg.c`.

## MP3 encoder

`encode(pcm,options,&bytes)` accepts mono/stereo at 32000, 44100 or 48000 Hz and
writes 320 kb/s MPEG-1 Layer III frames, long windows, independent stereo,
Huffman book1, no reservoir or CRC. It implements the polyphase analysis bank,
windowed MDCT, alias compensation, per-band scalefactors and spectral coding.
The compact binary spectral quantizer only codes magnitudes 0 and 1, adjusting
gain/scalefactors for range. **This is a low-fidelity baseline encoder**, not a
replacement for psychoacoustic encoders. It clips source samples to [-1,1].

Whole-frame padding and filterbank delay are retained. No Xing/LAME gapless
metadata or exact original length is stored. Output has
`ceil((input_frames+1152)/1152)` frames of 1152 PCM samples/channel. Empty input
produces a valid silent MP3 frame. Decode does not remove priming or padding.

## MP3 decoder

`decode(bytes,size,options,&pcm)` supports MPEG-1 Layer III, the three rates,
CBR/VBR standard indexed bitrates, mono/stereo/dual-channel and mid-side stereo,
long blocks, scalefactor reuse, pre-emphasis scalefactors and Huffman books 0/1.
There must be no count1-coded region. It implements requantization, alias
reduction, overlap-add IMDCT, frequency inversion and the synthesis filterbank.

**Arbitrary commercial MP3 files are not supported.** Other Huffman books,
short/switched blocks, bit reservoir use, protected CRC frames, intensity stereo,
free bitrate, de-emphasis and MPEG-2/2.5 return `UNSUPPORTED`. Format/rate/channel changes
within a stream are rejected. ID3v2 syncsafe-length tags and trailing ID3v1 tags
are skipped. Truncation and impossible lengths fail without committing PCM.

## MP3 SIMD, ownership and provenance

Both outputs must be zero initialized; use the audio free APIs. Failure leaves
output unchanged. Audio frame limits apply to padded decoded frames. SIMD
accelerates analysis, synthesis and MDCT dot products, selected through options;
entropy parsing remains scalar. Transform/history storage is per call, so calls
on independent buffers can run concurrently.

Algorithms and tables follow ISO/IEC 11172-3:1993, Layer III and Annex B.
The synthesis prototype integers are standardized Table B.3 coefficients,
mirrored at 256 and sign-alternated in 64-element groups; analysis uses the
corresponding window scaled by 1/32. The numeric prototype was cross-checked
against the `intwinbase` data in
<https://github.com/madebr/mpg123/blob/master/src/libmpg123/tabinit.c>.
No mpg123 implementation routines or other third-party codec code are included
or linked by the MP3 path. Optional MP3 interoperability tests use FFmpeg as
an independent oracle and compare PCM samples, not only container acceptance.

## MPEG-1 video profile

Video uses a raw ISO/IEC 11172-2 elementary stream (`.m1v`), not an MPEG
program/transport stream or another container. It supports progressive 8-bit
4:2:0 Y, Cb, Cr (I420), even coded dimensions from 2×2 to 768×576, and the
following exact frame rates: `24000/1001`, `24`, `25`, `30000/1001`, `30`,
`50`, `60000/1001` and `60` frames/s. The API does not convert color, pixels
or frame rates. Pass video-range samples; geometry is reported in coded
pixels, without aspect-ratio rescaling or display metadata.

- Encoding is lossy. It writes square-pixel I/P video with GOP target 12,
  a fixed quantizer setting and a terminating sequence-end code. It uses one
  software thread and bit-exact mode; repeated encodes with the same linked
  build produce the same stream. Bitstream identity across FFmpeg versions
  or platforms is not guaranteed.
- Decoding accepts self-contained I/P/B streams and returns pictures in
  display order, including delayed pictures at EOF. Input starts with a
  sequence header; repeated same-size/rate headers and a missing final
  sequence-end code are allowed. A present sequence-end code may only be
  followed by zero stuffing. Resolution or rate changes, MPEG-2 extensions,
  D-pictures and containers are outside this profile. An MPEG-1 stream
  without a sequence-end code cannot distinguish a deliberately absent
  suffix of whole pictures from a complete stream; this is not a general
  bitstream conformance validator.

### Video API and resource limits

`mulib_minimpeg_video_encode(format, frames, frame_count, limits, &bytes)`
takes borrowed frame views with three independently strided planes. Each
plane must expose at least `stride*(rows-1)+row_bytes` bytes; neither the
views nor the pixel buffers are retained. It returns owned elementary-stream
bytes, released with `mulib_audio_bytes_free`.

`mulib_minimpeg_video_decode(stream, size, limits, &clip)` returns an owned
packed allocation, released with `mulib_minimpeg_video_free`. Frame `i` starts
at `clip.data + i*clip.frame_size`. With `y = width*height`, Cb starts at offset
`y` and Cr at `y+y/4` within the frame. Frame rates are returned in canonical
rational form. Zero-initialize both kinds of output before use. A failed
operation leaves the output untouched.

These are synchronous, bounded whole-clip operations, not an incremental
demuxer. `NULL` limits, or zero for an individual limit, select 4096
pictures, 64 MiB of compressed stream and 64 MiB of packed pixels. Encoding
applies the raw limit to the useful samples, not caller-owned row padding.
The compressed cap cannot exceed FFmpeg's signed parser length less its
padding requirement. Decode checks dimensions, counts and sizes before
allocating output. It also holds a padded copy of the compressed data;
backend packet and reference-frame scratch memory is additional. Independent
calls do not share codec state.

Video statuses distinguish bad arguments (`INVALID`), malformed streams
(`FORMAT`), out-of-profile features (`UNSUPPORTED`), resource caps (`LIMIT`),
allocation failure (`MEMORY`), missing backend/components (`UNAVAILABLE`)
and unexpected native failures (`BACKEND`). The backend can issue its normal
FFmpeg diagnostics; the library does not set the global FFmpeg log policy.

### Video backend and tests

In the `MULIB_IMPL_MINIMPEG` translation unit, additionally define
`MULIB_MINIMPEG_USE_LIBAVCODEC`. Enable the MuLIB `MULIB_IMPL_CODEC_CORE` and
`MULIB_IMPL_BUFFER` implementations once in the program. Link
`libavcodec` (major 59 or newer, FFmpeg 5+) and `libavutil` using their public
development headers. The implementation does not start an executable,
perform a PATH lookup, or load libraries manually. Without the opt-in macro,
the video functions link but return `UNAVAILABLE`. With it, check the
`MULIB_MINIMPEG_VIDEO_ENCODER` and `MULIB_MINIMPEG_VIDEO_DECODER` bits returned
by `mulib_minimpeg_video_available()`. A minimal FFmpeg build needs the
software `mpeg1video` encoder for encoding, and the `mpeg1video` decoder
and `mpegvideo` parser for decoding.

Run the dedicated video test from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
  -DMULIB_MINIMPEG_USE_LIBAVCODEC tests/minimpeg-video.c \
  $(pkg-config --cflags --libs libavcodec libavutil) -lm -o minimpeg-video
./minimpeg-video
```

The same source built without the backend macro or FFmpeg flags verifies
the disabled video API and dependency-free MP3 operation. For external
checks the opt-in executable has fixed-format (64×48, 25 fps) commands:
`encode IN.yuv OUT.m1v`, `decode IN.m1v OUT.yuv`, `compare REF.yuv OUT.yuv`
(per-plane mean absolute error below five units for every frame), and
`fixture OUT.m1v SOURCE.yuv` (generate a 15-frame test clip).
`make check-minimpeg-video` also tests interoperability with libmpeg2's
`mpeg2dec` and mjpegtools' `mpeg2enc` when both are available. CMake uses the
opt-in `MULIB_BUILD_MINIMPEG_VIDEO_TESTS` option for the same tests.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-minimpeg.h`](../kernels/mulib-minimpeg.h) |
| Implementation switch | `MULIB_IMPL_MINIMPEG` |
| Repository-local includes | [mulib-audio](mulib-audio.md), [mulib-codec-core](mulib-codec-core.md) |
| Complete example | [`examples/modules/minimpeg.c`](../examples/modules/minimpeg.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/minimpeg.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
