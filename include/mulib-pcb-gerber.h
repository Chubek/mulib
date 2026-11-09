/** @file mulib-pcb-gerber.h
 * @brief Restricted RS-274X Gerber trace layer output: millimetres, leading-absolute 4.6
 * coordinates, circular D-code apertures, straight tracks only.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_GERBER in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_GERBER_H
#define MULIB_PCB_GERBER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-pcb-geometry.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_gerber_write(FILE *fp, const mulib_pcb_segment_t *traces, size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_GERBER_H */
#if defined(MULIB_IMPL_PCB_GERBER) && !defined(MULIB_PCB_GERBER_H_IMPLEMENTED)
#define MULIB_PCB_GERBER_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_gerber_write(FILE *fp, const mulib_pcb_segment_t *traces, size_t n) {
    if (!fp || (!traces && n))
        return 0;
    if (n > 990)
        return 0;
    for (size_t i = 0; i < n; i++) {
        const mulib_pcb_segment_t *s = &traces[i];
        if (!isfinite(s->width) || s->width <= 0 || !isfinite(s->a.x) || !isfinite(s->a.y) ||
            !isfinite(s->b.x) || !isfinite(s->b.y) || s->width > 1000 || fabs(s->a.x) > 1000 ||
            fabs(s->b.x) > 1000 || fabs(s->a.y) > 1000 || fabs(s->b.y) > 1000)
            return 0;
    }
    if (fputs("G04 Amb3r minimal Gerber X2 compatible RS-274X trace "
              "layer*\n%FSLAX46Y46*%\n%MOMM*%\n%LPD*%\nG01*\n",
              fp) == EOF)
        return 0;
    for (size_t i = 0; i < n; i++) {
        const mulib_pcb_segment_t *s = &traces[i];

        /* One aperture definition per trace, D10..D999. */
        int ap = (int)i + 10;
        if (fprintf(fp, "%%ADD%dC,%.6f*%%\nD%d*\nX%lldY%lldD02*\nX%lldY%lldD01*\n", ap, s->width,
                    ap, llround(s->a.x * 1e6), llround(s->a.y * 1e6), llround(s->b.x * 1e6),
                    llround(s->b.y * 1e6)) < 0)
            return 0;
    }
    return fputs("M02*\n", fp) != EOF;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_GERBER */
