/** @file mulib-immgtk-drawlist.h
 * @brief Backend-neutral owned draw-command array for lines, rectangles and circles; renderer
 * retains clipping and rasterization responsibilities.
 * @details C11 single-header implementation; define MULIB_IMPL_IMMGTK_DRAWLIST in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_IMMGTK_DRAWLIST_H
#define MULIB_IMMGTK_DRAWLIST_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_immgtk_primitive {
    MULIB_IMMGTK_LINE = 1,
    MULIB_IMMGTK_RECT = 2,
    MULIB_IMMGTK_CIRCLE = 3
} mulib_immgtk_primitive_t;
typedef struct mulib_immgtk_drawcmd {
    mulib_immgtk_primitive_t type;
    double x1, y1, x2, y2, width;
    uint32_t rgba;
} mulib_immgtk_drawcmd_t;
typedef struct mulib_immgtk_drawlist {
    mulib_immgtk_drawcmd_t *cmds;
    size_t count, cap;
} mulib_immgtk_drawlist_t;
int mulib_immgtk_drawlist_push(mulib_immgtk_drawlist_t *d, mulib_immgtk_drawcmd_t command);
void mulib_immgtk_drawlist_clear(mulib_immgtk_drawlist_t *d);
void mulib_immgtk_drawlist_free(mulib_immgtk_drawlist_t *d);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMMGTK_DRAWLIST_H */
#if defined(MULIB_IMPL_IMMGTK_DRAWLIST) && !defined(MULIB_IMMGTK_DRAWLIST_H_IMPLEMENTED)
#define MULIB_IMMGTK_DRAWLIST_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_immgtk_drawlist_push(mulib_immgtk_drawlist_t *d, mulib_immgtk_drawcmd_t c) {
    if (!d || c.type < MULIB_IMMGTK_LINE || c.type > MULIB_IMMGTK_CIRCLE || !isfinite(c.x1) ||
        !isfinite(c.x2) || !isfinite(c.y1) || !isfinite(c.y2) || !isfinite(c.width) || c.width < 0)
        return 0;
    if (d->count == d->cap) {
        size_t n = d->cap ? d->cap * 2 : 64;
        if (n <= d->cap || n > SIZE_MAX / sizeof *d->cmds)
            return 0;
        void *p = realloc(d->cmds, n * sizeof *d->cmds);
        if (!p)
            return 0;
        d->cmds = p;
        d->cap = n;
    }
    d->cmds[d->count++] = c;
    return 1;
}
void mulib_immgtk_drawlist_clear(mulib_immgtk_drawlist_t *d) {
    if (d)
        d->count = 0;
}
void mulib_immgtk_drawlist_free(mulib_immgtk_drawlist_t *d) {
    if (!d)
        return;
    free(d->cmds);
    memset(d, 0, sizeof *d);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_IMMGTK_DRAWLIST */
