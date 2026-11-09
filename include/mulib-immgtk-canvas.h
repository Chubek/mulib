/** @file mulib-immgtk-canvas.h
 * @brief GUI-backend-neutral 2D pan/zoom transforms with cursor-centered zoom and reciprocal
 * coordinate mapping.
 * @details C11 single-header implementation; define MULIB_IMPL_IMMGTK_CANVAS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_IMMGTK_CANVAS_H
#define MULIB_IMMGTK_CANVAS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_immgtk_canvas {
    double origin_x, origin_y, scale, offset_x, offset_y;
} mulib_immgtk_canvas_t;
int mulib_immgtk_canvas_world_to_screen(const mulib_immgtk_canvas_t *c, double x, double y,
                                        double *sx, double *sy);
int mulib_immgtk_canvas_screen_to_world(const mulib_immgtk_canvas_t *c, double x, double y,
                                        double *wx, double *wy);
int mulib_immgtk_canvas_zoom_at(mulib_immgtk_canvas_t *c, double screen_x, double screen_y,
                                double multiplier);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMMGTK_CANVAS_H */
#if defined(MULIB_IMPL_IMMGTK_CANVAS) && !defined(MULIB_IMMGTK_CANVAS_H_IMPLEMENTED)
#define MULIB_IMMGTK_CANVAS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_immgtk_canvas_world_to_screen(const mulib_immgtk_canvas_t *c, double x, double y,
                                        double *sx, double *sy) {
    if (!c || !sx || !sy || c->scale <= 0 || !isfinite(c->scale))
        return 0;
    double a = (x - c->origin_x) * c->scale + c->offset_x,
           b = (y - c->origin_y) * c->scale + c->offset_y;
    if (!isfinite(a) || !isfinite(b))
        return 0;
    *sx = a;
    *sy = b;
    return 1;
}
int mulib_immgtk_canvas_screen_to_world(const mulib_immgtk_canvas_t *c, double x, double y,
                                        double *wx, double *wy) {
    if (!c || !wx || !wy || c->scale <= 0 || !isfinite(c->scale))
        return 0;
    double a = (x - c->offset_x) / c->scale + c->origin_x,
           b = (y - c->offset_y) / c->scale + c->origin_y;
    if (!isfinite(a) || !isfinite(b))
        return 0;
    *wx = a;
    *wy = b;
    return 1;
}
int mulib_immgtk_canvas_zoom_at(mulib_immgtk_canvas_t *c, double sx, double sy, double m) {
    if (!c || !isfinite(m) || m <= 0 || !isfinite(sx) || !isfinite(sy))
        return 0;
    double wx, wy;
    if (!mulib_immgtk_canvas_screen_to_world(c, sx, sy, &wx, &wy))
        return 0;
    mulib_immgtk_canvas_t next = *c;
    next.scale *= m;
    if (next.scale < 1e-9 || next.scale > 1e9)
        return 0;
    next.offset_x = sx - (wx - next.origin_x) * next.scale;
    next.offset_y = sy - (wy - next.origin_y) * next.scale;
    if (!isfinite(next.offset_x) || !isfinite(next.offset_y))
        return 0;
    *c = next;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_IMMGTK_CANVAS */
