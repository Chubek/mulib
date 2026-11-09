/** @file mulib-pcb-excellon.h
 * @brief Minimal Excellon-style metric coordinate drill output, one tool number per hole.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_EXCELLON in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_EXCELLON_H
#define MULIB_PCB_EXCELLON_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_drill {
    double x, y, diameter;
} mulib_pcb_drill_t;
int mulib_pcb_excellon_write(FILE *fp, const mulib_pcb_drill_t *holes, size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_EXCELLON_H */
#if defined(MULIB_IMPL_PCB_EXCELLON) && !defined(MULIB_PCB_EXCELLON_H_IMPLEMENTED)
#define MULIB_PCB_EXCELLON_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_excellon_write(FILE *fp, const mulib_pcb_drill_t *h, size_t n) {
    if (!fp || (!h && n) || n > 9999)
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(h[i].diameter) || h[i].diameter <= 0 || !isfinite(h[i].x) ||
            !isfinite(h[i].y) || fabs(h[i].x) > 1000 || fabs(h[i].y) > 1000)
            return 0;
    }
    if (fputs("M48\nMETRIC,TZ\n", fp) == EOF)
        return 0;
    for (size_t i = 0; i < n; i++) {

        if (fprintf(fp, "T%02zuC%.3f\n", i + 1, h[i].diameter) < 0)
            return 0;
    }
    if (fputs("%\nG90\n", fp) == EOF)
        return 0;
    for (size_t i = 0; i < n; i++)
        if (fprintf(fp, "T%02zu\nX%.3fY%.3f\n", i + 1, h[i].x, h[i].y) < 0)
            return 0;
    return fputs("M30\n", fp) != EOF;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_EXCELLON */
