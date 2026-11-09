/** @file mulib-pcb-geometry.h
 * @brief 2D Euclidean point-to-segment and segment-to-segment distances and centerline clearance
 * for straight copper tracks with width and net IDs.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_GEOMETRY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_GEOMETRY_H
#define MULIB_PCB_GEOMETRY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_point {
    double x, y;
} mulib_pcb_point_t;
typedef struct mulib_pcb_segment {
    mulib_pcb_point_t a, b;
    double width;
    uint32_t net;
} mulib_pcb_segment_t;
double mulib_pcb_point_segment_distance(mulib_pcb_point_t p, mulib_pcb_point_t a,
                                        mulib_pcb_point_t b);
double mulib_pcb_segment_distance(mulib_pcb_point_t a, mulib_pcb_point_t b, mulib_pcb_point_t c,
                                  mulib_pcb_point_t d);
int mulib_pcb_segments_violate(mulib_pcb_segment_t a, mulib_pcb_segment_t b, double clearance);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_GEOMETRY_H */
#if defined(MULIB_IMPL_PCB_GEOMETRY) && !defined(MULIB_PCB_GEOMETRY_H_IMPLEMENTED)
#define MULIB_PCB_GEOMETRY_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
static double mulib_pcb_cross_(mulib_pcb_point_t a, mulib_pcb_point_t b, mulib_pcb_point_t c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
static int mulib_pcb_between_(mulib_pcb_point_t a, mulib_pcb_point_t b, mulib_pcb_point_t c) {
    return c.x >= fmin(a.x, b.x) && c.x <= fmax(a.x, b.x) && c.y >= fmin(a.y, b.y) &&
           c.y <= fmax(a.y, b.y);
}
double mulib_pcb_point_segment_distance(mulib_pcb_point_t p, mulib_pcb_point_t a,
                                        mulib_pcb_point_t b) {
    double dx = b.x - a.x, dy = b.y - a.y, d = dx * dx + dy * dy;
    if (d == 0)
        return hypot(p.x - a.x, p.y - a.y);
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / d;
    t = fmax(0, fmin(1, t));
    return hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}
double mulib_pcb_segment_distance(mulib_pcb_point_t a, mulib_pcb_point_t b, mulib_pcb_point_t c,
                                  mulib_pcb_point_t d) {
    double a1 = mulib_pcb_cross_(a, b, c), a2 = mulib_pcb_cross_(a, b, d),
           b1 = mulib_pcb_cross_(c, d, a), b2 = mulib_pcb_cross_(c, d, b);
    if (((a1 > 0 && a2 < 0) || (a1 < 0 && a2 > 0)) && ((b1 > 0 && b2 < 0) || (b1 < 0 && b2 > 0)))
        return 0;
    if (a1 == 0 && mulib_pcb_between_(a, b, c))
        return 0;
    if (a2 == 0 && mulib_pcb_between_(a, b, d))
        return 0;
    if (b1 == 0 && mulib_pcb_between_(c, d, a))
        return 0;
    if (b2 == 0 && mulib_pcb_between_(c, d, b))
        return 0;
    double x = mulib_pcb_point_segment_distance(a, c, d),
           y = mulib_pcb_point_segment_distance(b, c, d),
           z = mulib_pcb_point_segment_distance(c, a, b),
           w = mulib_pcb_point_segment_distance(d, a, b);
    return fmin(fmin(x, y), fmin(z, w));
}
int mulib_pcb_segments_violate(mulib_pcb_segment_t a, mulib_pcb_segment_t b, double clearance) {
    if (a.net == b.net || clearance < 0 || a.width < 0 || b.width < 0 || !isfinite(clearance) ||
        !isfinite(a.width) || !isfinite(b.width))
        return 0;
    return mulib_pcb_segment_distance(a.a, a.b, b.a, b.b) < clearance + .5 * (a.width + b.width);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_GEOMETRY */
