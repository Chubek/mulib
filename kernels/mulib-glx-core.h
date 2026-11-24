/* MuLIB graphical extension: glx-core. C11, header-only. */
#ifndef MULIB_GLX_CORE_H
#define MULIB_GLX_CORE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
/* Half-open rectangles; native uint32_t straight 0xAARRGGBB; word strides.
 * Owning surfaces must not be copied. Initialize into empty objects and release
 * before reinitializing. Borrowed storage must outlive every surface/view/user.
 * The legacy layout is retained: it cannot remember or discover allocation size.
 */
typedef struct { double x, y; } mgx_point;
typedef struct { int x, y, w, h; } mgx_rect;
typedef struct { double a, b, c, d, e, f; } mgx_affine;
typedef struct {
    uint32_t *pixels;
    size_t stride;
    int w, h, owned;
    mgx_rect clip;
} mgx_surface;
typedef mgx_point mulib_glx_point_t;
typedef mgx_rect mulib_glx_rect_t;
typedef mgx_affine mulib_glx_affine_t;
typedef mgx_surface mulib_glx_surface_t;

static inline int mgx_grow(void **p, size_t *cap, size_t need, size_t elem)
{
    size_t n;
    void *q;
    if (!p || !cap || !elem || *cap > SIZE_MAX / elem ||
        need > SIZE_MAX / elem || (*cap && !*p))
        return 0;
    if (need <= *cap)
        return 1;
    n = *cap ? *cap : 8;
    while (n < need) {
        if (n > SIZE_MAX / 2) {
            n = need;
            break;
        }
        n *= 2;
    }
    if (n > SIZE_MAX / elem)
        n = need;
    q = realloc(*p, n * elem);
    if (!q)
        return 0;
    *p = q;
    *cap = n;
    return 1;
}

static inline double mgx_clamp(double x, double a, double b)
{
    return x < a ? a : x > b ? b : x;
}

static inline int mgx_int(double x)
{
    return isnan(x) ? 0 : x >= INT_MAX ? INT_MAX :
           x <= INT_MIN ? INT_MIN : (int)x;
}

static inline int mgx_min(int a, int b) { return a < b ? a : b; }
static inline int mgx_max(int a, int b) { return a > b ? a : b; }

static inline mgx_rect mgx_intersect(mgx_rect a, mgx_rect b)
{
    int64_t x = mgx_max(a.x, b.x), y = mgx_max(a.y, b.y);
    int64_t ax = (int64_t)a.x + mgx_max(0, a.w);
    int64_t bx = (int64_t)b.x + mgx_max(0, b.w);
    int64_t ay = (int64_t)a.y + mgx_max(0, a.h);
    int64_t by = (int64_t)b.y + mgx_max(0, b.h);
    int64_t w = (ax < bx ? ax : bx) - x;
    int64_t h = (ay < by ? ay : by) - y;
    /* A positive intersection is no wider/taller than either input. */
    return (mgx_rect){(int)x, (int)y, w > 0 ? (int)w : 0,
                      h > 0 ? (int)h : 0};
}

static inline int mgx_contains(mgx_rect r, double x, double y)
{
    return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y &&
           x < (double)r.x + r.w && y < (double)r.y + r.h;
}

/* Minimum accessible word span, excluding padding after the final row. */
static inline int mgx_surface_required_words(size_t stride, int w, int h,
                                            size_t *words)
{
    size_t rows, limit = SIZE_MAX / sizeof(uint32_t);
    if (!words || w <= 0 || h <= 0 || stride < (size_t)w ||
        (size_t)w > limit)
        return 0;
    rows = (size_t)h - 1;
    if (rows && stride > (limit - (size_t)w) / rows)
        return 0;
    *words = rows * stride + (size_t)w;
    return 1;
}

/* Validates layout/address arithmetic, NOT the size of an arbitrary pointer. */
static inline int mgx_surface_valid(const mgx_surface *s)
{
    size_t words;
    return s && s->pixels &&
           (uintptr_t)s->pixels % _Alignof(uint32_t) == 0 &&
           mgx_surface_required_words(s->stride, s->w, s->h, &words) &&
           words <= (UINTPTR_MAX - (uintptr_t)s->pixels) / sizeof(uint32_t);
}

static inline mgx_rect mgx_surface_clip(const mgx_surface *s)
{
    return mgx_surface_valid(s) ?
           mgx_intersect(s->clip, (mgx_rect){0, 0, s->w, s->h}) :
           (mgx_rect){0, 0, 0, 0};
}

/* Capacity is measured in uint32_t words. Failure leaves *s unchanged. */
static inline int mgx_wrap_capacity(mgx_surface *s, uint32_t *p,
                                    size_t capacity, size_t stride, int w, int h)
{
    size_t words;
    mgx_surface t = {p, stride, w, h, 0, {0, 0, w, h}};
    if (!s || !mgx_surface_required_words(stride, w, h, &words) ||
        words > capacity || !mgx_surface_valid(&t))
        return 0;
    *s = t;
    return 1;
}

/* Compatibility entry point: caller guarantees the minimum accessible span. */
static inline int mgx_wrap(mgx_surface *s, uint32_t *p, size_t stride, int w, int h)
{
    size_t words;
    return mgx_surface_required_words(stride, w, h, &words) &&
           mgx_wrap_capacity(s, p, words, stride, w, h);
}

static inline int mgx_surface_init(mgx_surface *s, int w, int h)
{
    size_t words;
    uint32_t *p;
    if (!s || !mgx_surface_required_words((size_t)w, w, h, &words))
        return 0;
    p = (uint32_t *)calloc(words, sizeof *p);
    if (!p)
        return 0;
    /* The layout was checked above; calloc guarantees suitable alignment and
     * the complete allocation. Borrowed-pointer range checks are unnecessary
     * here and make some optimizers diagnose an impossible invalid-free path. */
    *s = (mgx_surface){p, (size_t)w, w, h, 1, {0, 0, w, h}};
    return 1;
}

static inline void mgx_surface_free(mgx_surface *s)
{
    if (s) {
        if (s->owned)
            free(s->pixels);
        memset(s, 0, sizeof *s);
    }
}

static inline unsigned mgx_ch(uint32_t c, int shift)
{
    return shift >= 0 && shift <= 24 ? (c >> shift) & 255u : 0;
}

static inline uint32_t mgx_over(uint32_t s, uint32_t d)
{
    unsigned a = s >> 24, b = d >> 24;
    unsigned alpha = a * 255u + b * (255u - a);
    uint32_t out;
    if (!alpha)
        return 0;
    out = ((alpha + 127u) / 255u) << 24;
    for (int k = 0; k < 24; k += 8) {
        unsigned n = mgx_ch(s, k) * a * 255u +
                     mgx_ch(d, k) * b * (255u - a);
        out |= ((n + alpha / 2u) / alpha) << k;
    }
    return out;
}

static inline uint32_t mgx_alpha(uint32_t c, double a)
{
    if (!isfinite(a))
        return c & 0xffffffu;
    return (c & 0xffffffu) |
           ((uint32_t)(mgx_clamp(a, 0, 1) * (c >> 24) + .5) << 24);
}

static inline void mgx_pixel(mgx_surface *s, int x, int y, uint32_t c)
{
    if (mgx_surface_valid(s) && x >= 0 && y >= 0 && x < s->w && y < s->h &&
        mgx_contains(s->clip, x, y)) {
        uint32_t *p = s->pixels + (size_t)y * s->stride + (size_t)x;
        *p = mgx_over(c, *p);
    }
}

static inline void mgx_fill(mgx_surface *s, mgx_rect r, uint32_t c)
{
    if (!mgx_surface_valid(s))
        return;
    r = mgx_intersect(r, mgx_surface_clip(s));
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++) {
            uint32_t *p = s->pixels + (size_t)y * s->stride + (size_t)x;
            *p = mgx_over(c, *p);
        }
}

/* Clear is SOURCE over the entire storage rectangle, independent of clip. */
static inline void mgx_clear(mgx_surface *s, uint32_t c)
{
    if (!mgx_surface_valid(s))
        return;
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
            s->pixels[(size_t)y * s->stride + (size_t)x] = c;
}

static inline double mgx_distseg(double x, double y, mgx_point a, mgx_point b)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    double px = x - a.x, py = y - a.y;
    double denom = dx * dx + dy * dy;
    double dot = px * dx + py * dy;
    double ax, ay, t, scale;
    /* Preserve the direct calculation's precision for ordinary coordinates. */
    if (isfinite(denom) && isfinite(dot)) {
        t = denom ? mgx_clamp(dot / denom, 0, 1) : 0;
        return hypot(px - t * dx, py - t * dy);
    }
    /* Normalize before subtracting/squaring to avoid finite-input overflow. */
    scale = fmax(fmax(fabs(x), fabs(y)),
                 fmax(fmax(fabs(a.x), fabs(a.y)),
                      fmax(fabs(b.x), fabs(b.y))));
    if (!isfinite(scale))
        return INFINITY;
    if (scale == 0)
        return 0;
    ax = a.x / scale;
    ay = a.y / scale;
    dx = b.x / scale - ax;
    dy = b.y / scale - ay;
    px = x / scale - ax;
    py = y / scale - ay;
    denom = dx * dx + dy * dy;
    t = denom ? mgx_clamp((px * dx + py * dy) / denom, 0, 1) : 0;
    return hypot(px - t * dx, py - t * dy) * scale;
}

static inline void mgx_line(mgx_surface *s, mgx_point a, mgx_point b,
                            double width, uint32_t c)
{
    mgx_rect clip;
    double pad;
    int x0, x1, y0, y1;
    if (!mgx_surface_valid(s) || !isfinite(width) || width <= 0 ||
        !isfinite(a.x) || !isfinite(a.y) ||
        !isfinite(b.x) || !isfinite(b.y))
        return;
    clip = mgx_surface_clip(s);
    pad = width / 2 + 1;
    x0 = mgx_max(clip.x, mgx_int(floor(fmin(a.x, b.x) - pad)));
    x1 = mgx_min(clip.x + clip.w, mgx_int(ceil(fmax(a.x, b.x) + pad)));
    y0 = mgx_max(clip.y, mgx_int(floor(fmin(a.y, b.y) - pad)));
    y1 = mgx_min(clip.y + clip.h, mgx_int(ceil(fmax(a.y, b.y) + pad)));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double v = mgx_clamp(width / 2 + .5 -
                                mgx_distseg(x + .5, y + .5, a, b), 0, 1);
            if (v > 0)
                mgx_pixel(s, x, y, mgx_alpha(c, v));
        }
}

static inline void mgx_circle(mgx_surface *s, double cx, double cy,
                              double r, uint32_t c)
{
    mgx_rect clip;
    int x0, x1, y0, y1;
    if (!mgx_surface_valid(s) || !isfinite(cx) || !isfinite(cy) ||
        !isfinite(r) || r <= 0)
        return;
    clip = mgx_surface_clip(s);
    x0 = mgx_max(clip.x, mgx_int(floor(cx - r - 1)));
    x1 = mgx_min(clip.x + clip.w, mgx_int(ceil(cx + r + 1)));
    y0 = mgx_max(clip.y, mgx_int(floor(cy - r - 1)));
    y1 = mgx_min(clip.y + clip.h, mgx_int(ceil(cy + r + 1)));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double v = mgx_clamp(r + .5 - hypot(x + .5 - cx, y + .5 - cy), 0, 1);
            if (v > 0)
                mgx_pixel(s, x, y, mgx_alpha(c, v));
        }
}

static inline uint64_t mgx_hash(const void *p, size_t n, uint64_t seed)
{
    const unsigned char *s = (const unsigned char *)p;
    uint64_t h = seed ? seed : UINT64_C(14695981039346656037);
    if (!p && n)
        return 0;
    while (n--) {
        h ^= *s++;
        h *= UINT64_C(1099511628211);
    }
    return h ? h : 1;
}

static inline mgx_point mgx_apply(mgx_affine m, mgx_point p)
{
    return (mgx_point){m.a * p.x + m.c * p.y + m.e,
                       m.b * p.x + m.d * p.y + m.f};
}

static inline mgx_affine mgx_mul(mgx_affine a, mgx_affine b)
{
    return (mgx_affine){a.a * b.a + a.c * b.b, a.b * b.a + a.d * b.b,
                        a.a * b.c + a.c * b.d, a.b * b.c + a.d * b.d,
                        a.a * b.e + a.c * b.f + a.e,
                        a.b * b.e + a.d * b.f + a.f};
}
#endif
