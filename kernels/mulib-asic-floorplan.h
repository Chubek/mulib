/** @file mulib-asic-floorplan.h
 * @brief Flat rectangular cell overlap test and deterministic single-row placement with spacing and
 * height validation.
 * @details C11 single-header implementation; define MULIB_IMPL_ASIC_FLOORPLAN in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_ASIC_FLOORPLAN_H
#define MULIB_ASIC_FLOORPLAN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_asic_rect {
    double x, y, width, height;
} mulib_asic_rect_t;
int mulib_asic_floorplan_overlaps(mulib_asic_rect_t a, mulib_asic_rect_t b);
int mulib_asic_floorplan_place_row(mulib_asic_rect_t *cells, size_t count, double start_x,
                                   double start_y, double spacing, double row_height);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ASIC_FLOORPLAN_H */
#if defined(MULIB_IMPL_ASIC_FLOORPLAN) && !defined(MULIB_ASIC_FLOORPLAN_H_IMPLEMENTED)
#define MULIB_ASIC_FLOORPLAN_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_asic_floorplan_overlaps(mulib_asic_rect_t a, mulib_asic_rect_t b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
           b.y < a.y + a.height;
}
int mulib_asic_floorplan_place_row(mulib_asic_rect_t *c, size_t n, double x, double y, double gap,
                                   double row) {
    if ((!c && n) || !isfinite(x) || !isfinite(y) || !isfinite(gap) || gap < 0 || !isfinite(row) ||
        row <= 0)
        return 0;
    double next = x;
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(c[i].width) || !isfinite(c[i].height) || c[i].width <= 0 ||
            c[i].height <= 0 || c[i].height > row || !isfinite(next + c[i].width + gap))
            return 0;
        next += c[i].width + gap;
    }
    for (size_t i = 0; i < n; i++) {
        c[i].x = x;
        c[i].y = y;
        x += c[i].width + gap;
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_ASIC_FLOORPLAN */
