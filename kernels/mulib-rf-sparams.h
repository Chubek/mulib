/** @file mulib-rf-sparams.h
 * @brief Single-port complex impedance/S11 conversion with scaled arithmetic and VSWR for
 * reflection magnitude below one.
 * @details C11 single-header implementation; define MULIB_IMPL_RF_SPARAMS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_RF_SPARAMS_H
#define MULIB_RF_SPARAMS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_rf_complex {
    double re, im;
} mulib_rf_complex_t;
int mulib_rf_s11_from_impedance(mulib_rf_complex_t impedance, double reference_ohm,
                                mulib_rf_complex_t *reflection);
int mulib_rf_impedance_from_s11(mulib_rf_complex_t reflection, double reference_ohm,
                                mulib_rf_complex_t *impedance);
int mulib_rf_s11_vswr(mulib_rf_complex_t reflection, double *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_RF_SPARAMS_H */
#if defined(MULIB_IMPL_RF_SPARAMS) && !defined(MULIB_RF_SPARAMS_H_IMPLEMENTED)
#define MULIB_RF_SPARAMS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_s11_from_impedance(mulib_rf_complex_t z, double z0, mulib_rf_complex_t *out) {
    if (!out || !isfinite(z.re) || !isfinite(z.im) || !isfinite(z0) || z0 <= 0)
        return 0;
    double scale = fmax(fmax(fabs(z.re), fabs(z.im)), z0), r = z.re / scale, i = z.im / scale,
           ref = z0 / scale;
    double c = r + ref, den = c * c + i * i;
    if (den == 0 || !isfinite(den))
        return 0;
    mulib_rf_complex_t v = {((r - ref) * c + i * i) / den, (2 * ref * i) / den};
    if (!isfinite(v.re) || !isfinite(v.im))
        return 0;
    *out = v;
    return 1;
}
int mulib_rf_impedance_from_s11(mulib_rf_complex_t g, double z0, mulib_rf_complex_t *out) {
    if (!out || !isfinite(g.re) || !isfinite(g.im) || !isfinite(z0) || z0 <= 0)
        return 0;
    double scale = fmax(1, fmax(fabs(g.re), fabs(g.im))), u = 1 / scale, r = g.re / scale,
           i = g.im / scale;
    double a = u + r, c = u - r, d = -i, den = c * c + d * d;
    if (!isfinite(den) || den == 0)
        return 0;
    mulib_rf_complex_t v = {((a * c + i * d) / den) * z0, ((i * c - a * d) / den) * z0};
    if (!isfinite(v.re) || !isfinite(v.im))
        return 0;
    *out = v;
    return 1;
}
int mulib_rf_s11_vswr(mulib_rf_complex_t g, double *out) {
    if (!out || !isfinite(g.re) || !isfinite(g.im))
        return 0;
    double magnitude = hypot(g.re, g.im);
    if (magnitude >= 1)
        return 0;
    double v = (1 + magnitude) / (1 - magnitude);
    if (!isfinite(v))
        return 0;
    *out = v;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_RF_SPARAMS */
