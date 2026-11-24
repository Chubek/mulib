/** @file mulib-pcb-pnp.h
 * @brief CSV pick-and-place placement export with millimetre XY, degrees and top/bottom side; no
 * footprint origins/centroid correction, assembly variant or machine-specific profile.
 * @details C11 single-header implementation; define MULIB_IMPL_PCB_PNP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_PCB_PNP_H
#define MULIB_PCB_PNP_H
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_pcb_placement {
    char reference[48];
    double x, y, rotation_deg;
    uint8_t side;
} mulib_pcb_placement_t;
int mulib_pcb_pnp_write(FILE *fp, const mulib_pcb_placement_t *parts, size_t count);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PCB_PNP_H */
#if defined(MULIB_IMPL_PCB_PNP) && !defined(MULIB_PCB_PNP_H_IMPLEMENTED)
#define MULIB_PCB_PNP_H_IMPLEMENTED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_pnp_write(FILE *fp, const mulib_pcb_placement_t *p, size_t n) {
    if (!fp || (!p && n))
        return 0;
    for (size_t i = 0; i < n; i++) {
        if (!memchr(p[i].reference, 0, sizeof p[i].reference) || !p[i].reference[0] ||
            strpbrk(p[i].reference, ",\"\r\n") || p[i].side > 1 || !isfinite(p[i].x) ||
            !isfinite(p[i].y) || !isfinite(p[i].rotation_deg))
            return 0;
    }
    if (fputs("Reference,X_mm,Y_mm,Rotation_deg,Side\n", fp) == EOF)
        return 0;
    for (size_t i = 0; i < n; i++) {

        if (fprintf(fp, "%s,%.6f,%.6f,%.6f,%s\n", p[i].reference, p[i].x, p[i].y, p[i].rotation_deg,
                    p[i].side ? "bottom" : "top") < 0)
            return 0;
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_PCB_PNP */
