/* C11 bridge: preserve legacy command storage and use the one shared renderer. */
#ifndef MULIB_IMMGTK_DRAWLIST_BRIDGE_H
#define MULIB_IMMGTK_DRAWLIST_BRIDGE_H
#include "mulib-immgtk-drawlist.h"
#include "mulib-glx-drawlist.h"
/** Append in order. Rect: opposite half-open corners, rounded outward.
 * Circle: center (x1,y1), radius width. Line: endpoints and stroke width.
 * rgba is passed unchanged as straight 0xAARRGGBB. Failure leaves dst->n unchanged.
 * Both lists must be separately owned, valid initialized objects. */
static inline int mulib_immgtk_drawlist_append(const mulib_immgtk_drawlist_t *src,
        mulib_glx_drawlist_t *dst, mgx_rect clip) {
    if (!src || !dst || src->count > src->cap || (src->count && !src->cmds) ||
        dst->n > dst->cap || (dst->cap && !dst->v) ||
        src->count > SIZE_MAX - dst->n) return 0;
    for (size_t i = 0; i < src->count; ++i) {
        const mulib_immgtk_drawcmd_t *c = src->cmds + i;
        if (c->type < MULIB_IMMGTK_LINE || c->type > MULIB_IMMGTK_CIRCLE ||
            !isfinite(c->x1) || !isfinite(c->y1) || !isfinite(c->x2) ||
            !isfinite(c->y2) || !isfinite(c->width) || c->width < 0) return 0;
        if (c->type == MULIB_IMMGTK_RECT &&
            (c->x1 < INT_MIN || c->y1 < INT_MIN || c->x2 > INT_MAX ||
             c->y2 > INT_MAX || c->x2 < c->x1 || c->y2 < c->y1 ||
             ceil(c->x2) - floor(c->x1) > INT_MAX ||
             ceil(c->y2) - floor(c->y1) > INT_MAX)) return 0;
    }
    if (!mgx_grow((void **)&dst->v, &dst->cap, dst->n + src->count,
            sizeof *dst->v)) return 0;
    for (size_t i = 0; i < src->count; ++i) {
        const mulib_immgtk_drawcmd_t *c = src->cmds + i;
        mgx_drawcmd out = {0};
        out.clip = clip;
        out.color = c->rgba;
        out.a = (mgx_point){c->x1, c->y1};
        out.b = (mgx_point){c->x2, c->y2};
        out.width = c->width;
        if (c->type == MULIB_IMMGTK_LINE) out.kind = MGX_LINE;
        else if (c->type == MULIB_IMMGTK_CIRCLE) out.kind = MGX_CIRCLE;
        else {
            out.kind = MGX_RECT;
            out.bounds = (mgx_rect){(int)floor(c->x1), (int)floor(c->y1),
                (int)(ceil(c->x2) - floor(c->x1)),
                (int)(ceil(c->y2) - floor(c->y1))};
        }
        dst->v[dst->n + i] = out;
    }
    dst->n += src->count;
    return 1;
}
#endif
