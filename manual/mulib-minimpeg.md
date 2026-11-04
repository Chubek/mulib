# mulib-minimpeg

Original MPEG-1 **Layer III (MP3)** encoding and profile-limited decoding;
this module does not implement MPEG video or MPEG audio Layers I/II.
Define `MULIB_IMPL_MINIMPEG`, `MULIB_IMPL_AUDIO`, `MULIB_IMPL_XSIMD`; link `-lm`
on Unix. Example: `examples/modules/minimpeg.c`.

## Encoder

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

## Decoder

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

## SIMD, ownership and provenance

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
or linked. Optional interoperability tests use FFmpeg solely as an independent
test oracle and compare PCM samples, not only container acceptance.
