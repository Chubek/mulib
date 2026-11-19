# mulib-imagepng

**Area:** Graphics  
**Implementation:** `MULIB_IMPL_IMAGEPNG` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Encode 8-bit RGB PNG using uncompressed DEFLATE blocks, or decode a bounded
noninterlaced 8-bit PNG into owned, straight RGBA pixels.

## API

```c
int mulib_imagepng_encode_rgb(const unsigned char *pixels,uint32_t width,uint32_t height,mulib_typecore_buf_t *out);
mulib_imagepng_limits_t mulib_imagepng_limits_default(void);
int mulib_imagepng_decode_rgba(const void *data, size_t size,
                             mulib_imagepng_rgba_t *out,
                             const mulib_imagepng_limits_t *limits);
void mulib_imagepng_rgba_free(mulib_imagepng_rgba_t *image);
```

## Behavior and limitations

The encoder retains its original RGB-only, uncompressed output.

The additive `static inline` decoder supports noninterlaced 8-bit grayscale,
RGB, indexed, grayscale/alpha and RGBA PNG, all five row filters, PLTE/tRNS,
and stored/fixed/dynamic DEFLATE through the existing MuLIB inflater. Every
chunk CRC and zlib Adler-32 is checked. The supported zlib wrapper advertises
a 32 KiB window. Packed/16-bit samples, Adam7, APNG and unknown critical chunks
are rejected; ancillary metadata is checked for CRC but not interpreted.
No color conversion or PNG-conformance certification is claimed.

Initialize `mulib_imagepng_rgba_t` empty and do not copy an owning image.
Successful decoding publishes tightly packed straight RGBA8 pixels, with
`stride` measured in bytes. Failed decoding leaves the destination unchanged.
Release the pixels with `mulib_imagepng_rgba_free`; input bytes are borrowed
only for the call.

Default limits are 128 MiB input, 16,384 pixels per dimension, 16,777,216
pixels, 64 MiB output, 65,536 chunks and 512 MiB variable working storage.
Use `mulib_imagepng_limits_default()` and adjust the resulting limits when
needed. Limits are inclusive; zero forbids work. The working bound excludes
caller-owned input, fixed stack storage and allocator bookkeeping. Match
`MULIB_IMAGEPNG_MALLOC`/`MULIB_IMAGEPNG_FREE` across image ownership boundaries.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Calling the decoder additionally requires the existing `MULIB_IMPL_BUFFER` and
`MULIB_IMPL_DEFLATE` implementations in exactly one C translation unit.
`MULIB_IMPL_IMAGEPNG` remains necessary only for the encoder; the new decoder
does not require another implementation switch or a system zlib library.
The opt-in GLX adapter and detailed malformed-input/ownership regressions are
documented in [GLX fonts and images](mulib-gfx-font-image.md#enabling-native-png).

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-imagepng.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-imagepng.c -lm -o test
./test
```

The decoder regression is `tests/test-gfx-font-image-followup.c` and also runs
through `make check-graphics`. It exercises filters/color types, checksums,
truncation, chunk ordering, exact resource limits and allocation failures.

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-imagepng.h`](../include/mulib-imagepng.h) |
| Implementation switch | `MULIB_IMPL_IMAGEPNG` |
| Repository-local includes | [mulib-deflate](mulib-deflate.md), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-imagepng.c`](../examples/mulib-imagepng.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-imagepng.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
