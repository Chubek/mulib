# GLX bitmap fonts and bounded native images

These C11 overlay APIs share `mgx_surface`. They do not require a display server,
font package, or third-party decoder. PPM/BMP remain dependency-free static-inline
paths. Native PNG is opt-in and reuses MuLIB's existing DEFLATE and checksum
kernels rather than adding a second inflater.

## Ownership

Initialize fonts and destination surfaces to zero. Constructors return `1` on
success and `0` on failure. They refuse to overwrite a live destination; free it
before reuse. Failure leaves the destination unchanged. Do not copy owning
fonts or surfaces, or mutate them concurrently.

```c
#include "mulib-glx-text.h"
#include "mulib-glx-imagedecode.h"

mulib_glx_font_t font = {0};
mgx_surface image = {0};
/* Check both results in an application. */
if (mulib_glx_font_builtin(&font) && mgx_surface_init(&image, 160, 40))
    mulib_glx_text_draw(&image, &font, 4, 4, "MuLIB", 5, 16, 0xffffffff);
mgx_surface_free(&image);
mulib_glx_font_free(&font);
```

`mulib_glx_font_psf2(&font, data, length)` copies both bitmap and mapping data.
The caller may release the input immediately after success. `font_free` accepts
NULL and repeated calls on the same cleared object. Original struct members and
function signatures are retained; mapping metadata is appended to the font
struct, so recompile consumers rather than assuming an old binary struct layout.
The retained `scale` member is metadata; text size is supplied explicitly.

## PSF2 and the built-in fallback

The loader validates PSF2 magic, version zero, supported flags, header length,
glyph storage, dimensions, and the complete Unicode table when flag 1 is set.
Glyphs may be 1–256 pixels in each dimension and use MSB-first bitmap rows.
Padding bytes in each glyph are allowed.

Unicode table entries use strict UTF-8, including two-, three-, and four-byte
scalars. Surrogates, overlong encodings, values above U+10FFFF, truncated data,
missing record terminators and surplus records are rejected. Multiple aliases
per glyph are supported. Duplicate aliases select the lowest glyph index.
Sequence mappings introduced by `0xfe` are validated (at least two scalars per
sequence), but **are not shaped**: their constituents do not become independent
aliases. Empty scalar lists and sequence-only glyphs are valid.

- `mulib_glx_font_lookup(font, codepoint, &glyph)` performs exact scalar lookup.
  A miss returns zero without changing `glyph`.
- `mulib_glx_font_glyph(font, codepoint)` falls back to U+FFFD, then `?`, then
  glyph zero. `UINT32_MAX` indicates an invalid font.
- `mulib_glx_font_bit` uses that fallback. `mulib_glx_font_glyph_bit` addresses a
  glyph index directly; out-of-range coordinates/indices produce zero.
- A PSF2 file **without** a Unicode table retains identity indexing. This is not
  a conversion from CP437 or any other historical encoding.

The default limits are 64 MiB of PSF2 input, 64 MiB of combined bitmap/mapping
allocations, and 1,048,576 scalar mapping entries before deduplication. Define
`MULIB_GLX_FONT_MAX_BYTES` and/or `MULIB_GLX_FONT_MAX_MAPPINGS` before including
the header to change them. Arithmetic checks still apply.

`mulib_glx_font_builtin` constructs the small original 6×8 ASCII fallback;
it does not load a system font. Missing non-ASCII characters render as `?`.
Neither loader provides TrueType/CFF outlines, hinting, BiDi, fallback font
selection, complex-script shaping, or general Unicode coverage.

## Text, cached glyphs and atlases

`mulib_glx_text_draw` borrows text and font only during the call. The text is
length-delimited UTF-8, not necessarily NUL-terminated. Invalid UTF-8 consumes
one byte and renders a replacement scalar, then continues; measurement uses the
same rule. LF starts a new line and CR is ignored.

Drawing uses nearest-neighbor bitmap scaling, an integral cell width
`max(1, floor(size * font_width / font_height))`, and size 1–1024. Measurement
uses the same cell-width rule, accepts finite positive sizes up to 1024, and
counts an empty string as one line. This fixes prior CR and fractional-advance
disagreement between measuring and drawing. Drawing clips before rasterization
and checks surface dimensions, stride arithmetic, and wide pen coordinates.
Wrapped surfaces must still refer to a genuinely large enough borrowed buffer;
the surface struct cannot prove an external allocation's capacity.

`mulib_glx_glyphcache_raster` requires an empty destination and returns an owned,
independent surface. `mulib_glx_textatlas_add` requires a backing surface at
least as large as the atlas, validates allocation before changing shelf state,
and writes the entire glyph cell as opaque white or transparent zero. Atlas
resource writes deliberately ignore the surface drawing clip; shelf padding
is reserved but left untouched. Cached and atlas pixels remain valid after the
source font is freed. Texture filtering users must initialize their own padding.

`mulib_glx_atlas_alloc` is a checked shelf allocator, not an eviction cache.
Start with positive `w`/`h`, nonnegative padding, and zero `x`/`y`/`row`. Failed
allocations leave both allocator state and output rectangle unchanged.

## Image decoding

`mulib_glx_imagedecode(data, length, &surface)` produces an owned surface of
straight native `uint32_t` pixels in `0xAARRGGBB` representation. Input is
borrowed only during the call. Supported formats:

- **P6 PPM:** ASCII header whitespace/comments, maxval 1–65535, and RGB raster
  data. Samples above 255 use two big-endian bytes. Samples are range-checked
  and rounded to 8-bit output. Exactly one whitespace byte follows maxval;
  whitespace-valued or `#` raster bytes are not skipped. Use a single LF there,
  not a CRLF pair. Trailing data may follow the first complete image.
- **BMP:** Windows DIB sizes 40, 52, 56, 108 and 124, one plane, positive width,
  positive bottom-up or negative top-down height, and uncompressed BI_RGB 24/32
  bits per pixel. Header extent, pixel offset, declared file/image sizes, padded
  row sizes and complete raster availability are checked. BI_RGB's reserved
  fourth byte is ignored; output is opaque, including for 32-bit BMP.
- **PNG, when enabled:** noninterlaced 8-bit grayscale, RGB, indexed color,
  grayscale+alpha and RGBA (color types 0, 2, 3, 4, 6). All five row filters,
  consecutive/split/empty IDAT chunks, stored/fixed/dynamic DEFLATE blocks,
  PLTE, and the corresponding grayscale/RGB/palette tRNS transparency are
  supported. Alpha is preserved without premultiplication, including RGB
  channels beneath zero alpha.

JPEG, WebP, compressed/paletted BMP and BMP bitfields are not decoded. PNG packed
1/2/4-bit or 16-bit samples, Adam7 and APNG are explicitly rejected. The supported
zlib wrapper advertises a 32 KiB window; smaller advertised windows are rejected
rather than accepting streams whose distances the existing raw decoder cannot
check against that smaller window. Unsupported formats return zero, not
fabricated pixels.

### Enabling native PNG

Define `MULIB_GLX_IMAGEDECODE_ENABLE_PNG` before including
`mulib-glx-imagedecode.h` in each consuming translation unit. Provide the existing
`MULIB_IMPL_TYPECORE`, `MULIB_IMPL_BUFFER`, and `MULIB_IMPL_DEFLATE` implementations
in **exactly one** translation unit, or link objects that already provide them:

```c
/* These three implementation switches belong in one .c file only. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_DEFLATE
#define MULIB_GLX_IMAGEDECODE_ENABLE_PNG
#include "mulib-glx-imagedecode.h"
```

`MULIB_IMPL_IMAGEPNG` is needed only when using the existing PNG **encoder**;
its original API and dependency contract are unchanged. Keeping PNG opt-in
preserves existing graphics consumers that intentionally link only libc/libm.
No zlib shared library or runtime loading is used.

The additive `mulib_imagepng_decode_rgba(data, length, &image, &limits)` helper in
`mulib-imagepng.h` is also available independently of GLX. It returns `1`/`0` and
publishes an owned, tightly packed RGBA8 image only on success. Initialize
`mulib_imagepng_rgba_t` to zero and release it with
`mulib_imagepng_rgba_free`; live outputs are rejected. Input is borrowed only
during the call. `MULIB_IMAGEPNG_MALLOC`/`MULIB_IMAGEPNG_FREE` customize the
decoder's own allocations; existing buffer allocations follow their own kernel's
allocation policy.

The parser checks the entire PNG signature, chunk extents/type characters,
reserved chunk bit, every chunk CRC, mandatory/duplicate chunk ordering,
consecutive IDAT ordering, palette indices, exact decompressed row count, zlib
FCHECK/no-dictionary, exact DEFLATE stream boundary, and Adler-32. Unknown
critical chunks and APNG animation chunks fail. Ancillary metadata is CRC-checked
but otherwise skipped, including color profiles, gamma, EXIF and text; no
color-space conversion or metadata extraction is claimed. The input must end
immediately after the zero-length IEND chunk.

The default limits are:

| Limit | Value |
| --- | --- |
| Input bytes | 128 MiB |
| Width and height | 16,384 each |
| Pixels | 16,777,216 |
| Output bytes | 64 MiB |

Use `mulib_glx_image_limits_default()` to obtain a
`mulib_glx_image_limits_t`, adjust it, then call
`mulib_glx_imagedecode_limited(data, length, &surface, &limits)`. All limits are
inclusive; a zero field forbids decoding. Passing NULL limits selects defaults.
Overflow and raster-bound checks remain mandatory even with raised limits.
All decoding happens into a temporary surface, so malformed samples or
allocation failures never leave a partially published destination.

The PNG helper additionally defaults to **65,536 chunks** and **512 MiB of
variable-size working allocations**, excluding caller-owned encoded input,
fixed stack storage and allocator bookkeeping. The conservative working bound
accounts for concatenated IDAT, both capacities used by the existing
failure-atomic inflater, and the RGBA output. Obtain
`mulib_imagepng_limits_default()` to adjust these ceilings when using the helper
directly. Every field is inclusive; zero forbids decoding. The GLX adapter maps
its five existing image limits to PNG limits and retains the default independent
PNG chunk/working ceilings. Converting RGBA to `mgx_surface` temporarily holds
both output representations, each bounded by `max_output_bytes`.

## Regression tests

From the repository root:

```sh
cc -std=c11 -pedantic -Wall -Wextra -Werror \
  -Ikernels tests/test-gfx-font-image.c \
  -lm -o /tmp/opencode/test-gfx-font-image
/tmp/opencode/test-gfx-font-image

cc -std=c11 -pedantic -Wall -Wextra -Werror \
  -g -O1 -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Ikernels tests/test-gfx-font-image.c \
  -lm -o /tmp/opencode/test-gfx-font-image-asan
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 /tmp/opencode/test-gfx-font-image-asan

cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Wpedantic -Werror \
  -Ikernels tests/test-gfx-font-image-followup.c \
  -lm -o /tmp/opencode/test-gfx-font-image-followup
/tmp/opencode/test-gfx-font-image-followup
```

The test build injects bitmap/map/surface allocation failures. It verifies exact pixels,
multi-byte aliases, fallback order, sequence exclusion, every truncated prefix,
malformed headers/UTF-8, 16-bit sample conversion, padded BMP rows, top-down
images, live-destination rejection, source lifetimes, clipping, allocator
overflow, and 3,000 deterministic parser mutations.

The follow-up suite checks all five supported PNG color types and all filters,
first-row filter boundaries, transparency and hidden RGB preservation, split
IDAT at every compressed-byte boundary, an independently generated dynamic
Huffman fixture, native encoder round trips, every truncated prefix, bad CRCs,
Adler/FCHECK/DEFLATE corruption, chunk ordering, malformed palettes,
exact dimension/output/working/chunk limits, each allocation failure through
GLX surface publication, and 4,000 deterministic mutations. Both tests pass
strict GCC and Clang C11 builds and ASan/UBSan. These are subset regressions,
not a PNG conformance, shaping, font-discovery or hardware-rendering certificate.
