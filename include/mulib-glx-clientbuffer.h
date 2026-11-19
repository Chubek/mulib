/* MuLIB notebook desktop extension. C11, header-only. SPDX-License-Identifier: MIT */
#ifndef MULIB_GLX_CLIENTBUFFER_H
#define MULIB_GLX_CLIENTBUFFER_H
#include "mulib-wm-core.h"
#include "mulib-glx-core.h"

/* Single-owner-thread, straight ARGB8888 view. The descriptor and storage must
 * remain at stable addresses until the final unref. Never copy a live owner.
 * release may free the descriptor itself; it must not reenter its owning scene. */
typedef void (*mulib_glx_clientbuffer_release_fn)(void *);
typedef struct {
    const uint32_t *pixels;
    size_t words, stride;
    int32_t w, h;
    unsigned refs;
    void *user;
    mulib_glx_clientbuffer_release_fn release;
} mulib_glx_clientbuffer;

static inline int mulib_glx_clientbuffer_valid(const mulib_glx_clientbuffer *b)
{
    size_t required;
    return b && b->refs && b->pixels &&
           (uintptr_t)b->pixels % _Alignof(uint32_t) == 0 &&
           mgx_surface_required_words(b->stride, b->w, b->h, &required) &&
           required <= b->words && b->words <= SIZE_MAX / sizeof(uint32_t) &&
           b->words <= (UINTPTR_MAX - (uintptr_t)b->pixels) / sizeof(uint32_t);
}

/* Output must be unused. The caller supplies the actual accessible capacity,
 * including only the final row's pixels (not necessarily its trailing padding). */
static inline int mulib_glx_clientbuffer_init(
    mulib_glx_clientbuffer *b, const uint32_t *p, size_t words, size_t stride,
    int32_t w, int32_t h, void *u, mulib_glx_clientbuffer_release_fn release)
{
    mulib_glx_clientbuffer t = {p, words, stride, w, h, 1, u, release};
    if (!b || !mulib_glx_clientbuffer_valid(&t))
        return 0;
    *b = t;
    return 1;
}

/* Explicit zero-copy adapter. A surface's owned flag is NOT transferred;
 * release/user must arrange that its allocation outlives all scene references.
 * Surface clip is view state, not part of the immutable buffer. */
static inline int mulib_glx_clientbuffer_from_surface(
    mulib_glx_clientbuffer *b, const mgx_surface *s, size_t words,
    void *u, mulib_glx_clientbuffer_release_fn release)
{
    return mgx_surface_valid(s) &&
           mulib_glx_clientbuffer_init(b, s->pixels, words, s->stride,
                                      s->w, s->h, u, release);
}

static inline int mulib_glx_clientbuffer_ref(mulib_glx_clientbuffer *b)
{
    if (!mulib_glx_clientbuffer_valid(b) || b->refs == UINT_MAX)
        return 0;
    b->refs++;
    return 1;
}

static inline void mulib_glx_clientbuffer_unref(mulib_glx_clientbuffer *b)
{
    if (b && b->refs && !--b->refs) {
        void *u = b->user;
        mulib_glx_clientbuffer_release_fn fn = b->release;
        memset(b, 0, sizeof *b);
        if (fn)
            fn(u);
        /* b may have been freed by fn. */
    }
}
#endif
