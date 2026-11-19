/* MuLIB graphical extension: glx-drawlist. C11, header-only. */
#ifndef MULIB_GLX_DRAWLIST_H
#define MULIB_GLX_DRAWLIST_H
#include "mulib-glx-texture.h"
#include "mulib-glx-shape.h"
typedef enum {
    MGX_RECT, MGX_LINE, MGX_CIRCLE, MGX_IMAGE, MGX_ROUNDRECT
} mgx_draw_kind;
typedef struct {
    mgx_draw_kind kind;
    mgx_rect bounds, clip;
    mgx_point a, b;
    double width;
    uint32_t color;
    const mgx_surface *image;
} mgx_drawcmd;
typedef mgx_draw_kind mulib_glx_draw_kind_t;
typedef mgx_drawcmd mulib_glx_drawcmd_t;
/* Command image structs AND their pixel storage are borrowed until rendering
 * finishes. Reset/release only frees the command array, never these resources.
 * Record/render on one thread; never mutate a list during rendering.
 */
typedef struct { mgx_drawcmd *v; size_t n, cap; } mulib_glx_drawlist_t;

static inline int mulib_glx_drawcmd_valid(const mgx_drawcmd *c)
{
    if (!c || c->kind < MGX_RECT || c->kind > MGX_ROUNDRECT)
        return 0;
    switch (c->kind) {
    case MGX_LINE:
        return isfinite(c->a.x) && isfinite(c->a.y) &&
               isfinite(c->b.x) && isfinite(c->b.y) &&
               isfinite(c->width) && c->width >= 0;
    case MGX_CIRCLE:
        return isfinite(c->a.x) && isfinite(c->a.y) &&
               isfinite(c->width) && c->width >= 0;
    case MGX_ROUNDRECT:
        return isfinite(c->width) && c->width >= 0;
    case MGX_IMAGE:
        return mgx_surface_valid(c->image);
    case MGX_RECT:
        return 1;
    }
    return 0;
}

static inline int mulib_glx_drawlist_valid(const mulib_glx_drawlist_t *d)
{
    if (!d || d->n > d->cap || d->cap > SIZE_MAX / sizeof *d->v ||
        (d->cap && !d->v))
        return 0;
    for (size_t i = 0; i < d->n; i++)
        if (!mulib_glx_drawcmd_valid(d->v + i))
            return 0;
    return 1;
}

static inline int mulib_glx_drawlist_add(mulib_glx_drawlist_t *d, mgx_drawcmd c)
{
    if (!d || d->n > d->cap || d->n == SIZE_MAX ||
        !mulib_glx_drawcmd_valid(&c) ||
        !mgx_grow((void **)&d->v, &d->cap, d->n + 1, sizeof c))
        return 0;
    d->v[d->n++] = c;
    return 1;
}

static inline void mulib_glx_drawlist_reset(mulib_glx_drawlist_t *d)
{
    if (d)
        d->n = 0;
}

static inline void mulib_glx_drawlist_release(mulib_glx_drawlist_t *d)
{
    if (d) {
        free(d->v);
        memset(d, 0, sizeof *d);
    }
}

/* Exact insertion order is painter order; no sorting/batching by texture/color.
 * Invalid lists are rejected before drawing. On a later image-copy allocation
 * failure earlier commands remain drawn; the caller's clip is always restored.
 */
static inline int mulib_glx_drawlist_render_checked(
    const mulib_glx_drawlist_t *d, mgx_surface *s)
{
    mgx_rect old, clip;
    int ok = 1;
    if (!mulib_glx_drawlist_valid(d) || !mgx_surface_valid(s))
        return 0;
    old = s->clip;
    clip = mgx_surface_clip(s);
    for (size_t i = 0; i < d->n && ok; i++) {
        const mgx_drawcmd *c = d->v + i;
        s->clip = mgx_intersect(clip, c->clip);
        switch (c->kind) {
        case MGX_RECT:
            mgx_fill(s, c->bounds, c->color);
            break;
        case MGX_LINE:
            mgx_line(s, c->a, c->b, c->width, c->color);
            break;
        case MGX_CIRCLE:
            mgx_circle(s, c->a.x, c->a.y, c->width, c->color);
            break;
        case MGX_IMAGE:
            ok = mulib_glx_texture_draw_checked(s, c->image, c->bounds, 1);
            break;
        case MGX_ROUNDRECT:
            mulib_glx_shape_roundrect(s, c->bounds, c->width, c->color);
            break;
        }
    }
    s->clip = old;
    return ok;
}

static inline void mulib_glx_drawlist_render(const mulib_glx_drawlist_t *d,
                                            mgx_surface *s)
{
    (void)mulib_glx_drawlist_render_checked(d, s);
}

/* Optional self-contained, owning frozen list. Every image is copied at capture,
 * not at execution. It may outlive the original commands/images. Do not copy or
 * mutate this struct or its draws/images; it must outlive queued submissions.
 */
typedef struct {
    mulib_glx_drawlist_t draws;
    mgx_surface *images;
    size_t image_count;
} mulib_glx_drawlist_snapshot_t;

static inline void mulib_glx_drawlist_snapshot_release(
    mulib_glx_drawlist_snapshot_t *snapshot)
{
    if (!snapshot)
        return;
    for (size_t i = 0; i < snapshot->image_count; i++)
        mgx_surface_free(snapshot->images + i);
    free(snapshot->images);
    mulib_glx_drawlist_release(&snapshot->draws);
    memset(snapshot, 0, sizeof *snapshot);
}

/* Initialize into an empty output. Failure leaves it unchanged. */
static inline int mulib_glx_drawlist_snapshot(
    mulib_glx_drawlist_snapshot_t *out, const mulib_glx_drawlist_t *d)
{
    mulib_glx_drawlist_snapshot_t t = {0};
    size_t images = 0, next = 0;
    if (!out || d == &out->draws || !mulib_glx_drawlist_valid(d))
        return 0;
    for (size_t i = 0; i < d->n; i++)
        if (d->v[i].kind == MGX_IMAGE)
            images++;
    if (images > SIZE_MAX / sizeof *t.images)
        return 0;
    if (d->n) {
        t.draws.v = (mgx_drawcmd *)malloc(d->n * sizeof *t.draws.v);
        if (!t.draws.v)
            return 0;
        memcpy(t.draws.v, d->v, d->n * sizeof *t.draws.v);
        t.draws.n = t.draws.cap = d->n;
    }
    if (images) {
        t.images = (mgx_surface *)calloc(images, sizeof *t.images);
        if (!t.images)
            goto fail;
    }
    for (size_t i = 0; i < d->n; i++) {
        if (d->v[i].kind != MGX_IMAGE)
            continue;
        if (!mulib_glx_surface_snapshot(t.images + next, d->v[i].image))
            goto fail;
        t.draws.v[i].image = t.images + next++;
        t.image_count = next;
    }
    *out = t;
    return 1;
fail:
    mulib_glx_drawlist_snapshot_release(&t);
    return 0;
}
#endif
