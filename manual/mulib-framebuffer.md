# mulib-framebuffer

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_FRAMEBUFFER` once; no dependencies or allocation.
[Example](../examples/modules/framebuffer.c).

Surface fields are a borrowed volatile 32-bit pixel pointer, total capacity in
words, visible width/height and row pitch in words. `valid` checks nonzero
dimensions, pitch, alignment and overflow-safe last visible pixel bounds.
The module only supports directly mapped 32-bit pixels; obtain and normalize
GOP/VBE format and mapping details in the application's firmware adapter.

`pattern` fills a two-color checkerboard with a nonzero tile size in pixels;
`verify` reads it back and counts mismatches. Both skip row padding. Colors are
raw device-format words. Pattern returns 0/-1; verify returns `SIZE_MAX` on
invalid input, otherwise mismatch count. Invalid calls never touch pixel storage.

**Destructive and CPU-driven**, not a shader/compute GPU stress test. A real
framebuffer must be mapped writable, and readable for verify. Caller handles
cache flushes, write-combining fences and display synchronization. Some apertures
do not support reliable readback. Never pass ordinary physical addresses without
mapping or assume this tests dedicated VRAM instead of a system-memory shadow.

`mulib_framebuffer_rgb` packs RGB8 into three caller-supplied firmware channel
masks, including RGB565 or RGB/BGR888. Masks must be contiguous, nonzero and
disjoint; conversion scales/rounds each channel using 64-bit arithmetic. Unused
bits are zero. Returns 0 and writes pixel, or -1 without modifying output.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-framebuffer.h`](../include/mulib-framebuffer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-framebuffer.h
 * @brief Bounded 32-bit linear framebuffer pattern fill/readback (CPU, not GPU compute).
 * Define MULIB_IMPL_FRAMEBUFFER once. Caller supplies mapped framebuffer and pitch.
 */
#ifndef MULIB_FRAMEBUFFER_H
#define MULIB_FRAMEBUFFER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { volatile uint32_t *pixels; size_t words, width, height, pitch_words; } mulib_framebuffer_t;
/** @brief Validate dimensions, capacity and alignment. Empty surfaces are rejected. */
int mulib_framebuffer_valid(const mulib_framebuffer_t *surface);
/** @brief Write a deterministic checkerboard without touching row padding. */
int mulib_framebuffer_pattern(const mulib_framebuffer_t *surface, uint32_t a, uint32_t b, size_t tile);
/** @brief Count differing pixels; SIZE_MAX invalid. Requires readable framebuffer.
 * Caller must synchronize mappings/caches before verification.
 */
size_t mulib_framebuffer_verify(const mulib_framebuffer_t *surface, uint32_t a, uint32_t b, size_t tile);
/** @brief Pack RGB8 into firmware channel masks. Masks must be contiguous,
 * nonzero and disjoint; unused/reserved bits are zero. Returns 0/-1.
 */
int mulib_framebuffer_rgb(uint8_t red, uint8_t green, uint8_t blue,
                           uint32_t red_mask, uint32_t green_mask, uint32_t blue_mask, uint32_t *pixel);
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
| Header | [`include/mulib-framebuffer.h`](../include/mulib-framebuffer.h) |
| Implementation switch | `MULIB_IMPL_FRAMEBUFFER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/framebuffer.c`](../examples/modules/framebuffer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/framebuffer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
