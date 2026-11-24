/** @file mulib-pcb-drc.h
 * @brief All-pairs straight-segment copper clearance checks for distinct nets.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_DRC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_DRC_H
#define MULIB_PCB_DRC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-pcb-geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_drc_violation {
    size_t first, second;
    double actual, required;
} mulib_pcb_drc_violation_t;
size_t mulib_pcb_drc_segments(const mulib_pcb_segment_t *tracks, size_t count, double clearance,
                              mulib_pcb_drc_violation_t *out, size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_DRC_H */
#if defined(MULIB_IMPL_PCB_DRC) && !defined(MULIB_PCB_DRC_H_IMPLEMENTED)
#define MULIB_PCB_DRC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_pcb_drc_segments(const mulib_pcb_segment_t *t, size_t n, double clearance,
                              mulib_pcb_drc_violation_t *out, size_t cap) {
    if ((!t && n) || !isfinite(clearance) || clearance < 0)
        return SIZE_MAX;
    for (size_t i = 0; i < n; i++)
        if (!isfinite(t[i].width) || t[i].width <= 0 || !isfinite(t[i].a.x) ||
            !isfinite(t[i].a.y) || !isfinite(t[i].b.x) || !isfinite(t[i].b.y))
            return SIZE_MAX;
    size_t hits = 0;
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            if (!mulib_pcb_segments_violate(t[i], t[j], clearance))
                continue;
            if (out && hits < cap) {
                out[hits].first = i;
                out[hits].second = j;
                out[hits].actual = mulib_pcb_segment_distance(t[i].a, t[i].b, t[j].a, t[j].b) -
                                   .5 * (t[i].width + t[j].width);
                out[hits].required = clearance;
            }
            hits++;
        }
    return hits;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_DRC */
