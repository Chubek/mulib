# mulib-audio

Common PCM, status, limits, encoded-byte storage and file IO for the audio
modules. Define `MULIB_IMPL_AUDIO` and `MULIB_IMPL_XSIMD`; include
`mulib-audio.h`. Uses only the C runtime and MuLIB. Example:
`examples/modules/audio.c`.

Zero-initialize `mulib_audio_pcm_t` and `mulib_audio_bytes_t`. PCM contains
interleaved floats, sample rate in Hz, 1..8 channels and a **frame** count
(one sample per channel). Nominal full scale is [-1,1], but finite values outside
that interval are legal for mixing; integer encoders clip. `pcm_create` allocates
silence with overflow checks, accepts zero frames, and requires empty output.
`pcm_free`/`bytes_free` release and zero their objects; both accept NULL.
Do not shallow-copy owned objects and then free both copies. Codecs borrow input
only for the call, allocate output, and leave output unchanged on error.

`validate` checks shape, all samples for finiteness, backend availability and
frame limit. Options zero-initialized means AUTO SIMD and 16,777,216 frames.
The limit applies to frames, not bytes; stereo float PCM needs eight bytes/frame.
Large limits may require substantial working storage. Allocation overflow is
checked separately. Set a smaller limit when processing untrusted files.

`append` grows an owned byte buffer; its source must not alias the buffer.
`read_file` reads a binary file, enforcing a byte limit (default 256 MiB).
`write_file` writes binary data with checked close/flush status; IO failure may
leave a partial file. File operations use standard `FILE` APIs, not subprocesses.

Statuses distinguish invalid API arguments/samples, malformed input, unsupported
features, resource limits, allocation, IO and script failures. `status_string`
returns static text. An unsupported SIMD selection is also `UNSUPPORTED`.
Objects require external synchronization if shared; independent objects can be
used concurrently.
