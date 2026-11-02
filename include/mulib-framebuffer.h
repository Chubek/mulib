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
#if defined(MULIB_IMPL_FRAMEBUFFER) && !defined(MULIB_FRAMEBUFFER_IMPLEMENTED)
#define MULIB_FRAMEBUFFER_IMPLEMENTED
int mulib_framebuffer_rgb(uint8_t r, uint8_t g, uint8_t b,
                           uint32_t rm, uint32_t gm, uint32_t bm, uint32_t *pixel) {
    uint32_t masks[3], value = 0, m; uint8_t colors[3]; unsigned i, shift;
    if (!pixel || !rm || !gm || !bm || (rm & gm) || (rm & bm) || (gm & bm)) return -1;
    masks[0] = rm; masks[1] = gm; masks[2] = bm; colors[0] = r; colors[1] = g; colors[2] = b;
    for (i = 0; i < 3; ++i) {
        m = masks[i]; shift = 0;
        while (!(m & 1)) { m >>= 1; ++shift; }
        if (m & (m + 1)) return -1;
        value |= (uint32_t)(((uint64_t)colors[i] * m + 127) / 255) << shift;
    }
    *pixel = value; return 0;
}
int mulib_framebuffer_valid(const mulib_framebuffer_t *s) {
    if (!s || !s->pixels || (uintptr_t)s->pixels % sizeof(uint32_t) || !s->width || !s->height ||
        s->pitch_words < s->width || s->words > SIZE_MAX / sizeof(uint32_t)) return 0;
    return s->width <= s->words && s->height - 1 <= (s->words - s->width) / s->pitch_words;
}
int mulib_framebuffer_pattern(const mulib_framebuffer_t *s, uint32_t a, uint32_t b, size_t tile) {
    size_t x, y; if (!tile || !mulib_framebuffer_valid(s)) return -1;
    for (y = 0; y < s->height; ++y) for (x = 0; x < s->width; ++x)
        s->pixels[y * s->pitch_words + x] = ((x / tile) ^ (y / tile)) & 1 ? b : a;
    return 0;
}
size_t mulib_framebuffer_verify(const mulib_framebuffer_t *s, uint32_t a, uint32_t b, size_t tile) {
    size_t x, y, errors = 0; uint32_t expected;
    if (!tile || !mulib_framebuffer_valid(s)) return SIZE_MAX;
    for (y = 0; y < s->height; ++y) for (x = 0; x < s->width; ++x) {
        expected = ((x / tile) ^ (y / tile)) & 1 ? b : a;
        if (s->pixels[y * s->pitch_words + x] != expected) ++errors;
    }
    return errors;
}
#endif
