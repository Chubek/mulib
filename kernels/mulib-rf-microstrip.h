/** @file mulib-rf-microstrip.h
 * @brief Quasi-static zero-thickness microstrip impedance estimate for width/substrate thickness
 * ratio and dielectric constant.
 * @details C11 single-header implementation; define MULIB_IMPL_RF_MICROSTRIP in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_RF_MICROSTRIP_H
#define MULIB_RF_MICROSTRIP_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_microstrip_z0(double width_mm, double substrate_mm, double epsilon_r,
                           double *impedance_ohm);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RF_MICROSTRIP_H */
#if defined(MULIB_IMPL_RF_MICROSTRIP) && !defined(MULIB_RF_MICROSTRIP_H_IMPLEMENTED)
#define MULIB_RF_MICROSTRIP_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_microstrip_z0(double w, double h, double er, double *out) {
    if (!out || !isfinite(w) || !isfinite(h) || !isfinite(er) || w <= 0 || h <= 0 || er <= 1)
        return 0;
    double u = w / h,
           ee = (er + 1) * .5 +
                (er - 1) * .5 * (1 / sqrt(1 + 12 / u) + (u < 1 ? .04 * (1 - u) * (1 - u) : 0));
    double z = u <= 1
                   ? (60 / sqrt(ee)) * log(8 / u + u / 4)
                   : (120 * 3.141592653589793 / (sqrt(ee) * (u + 1.393 + .667 * log(u + 1.444))));
    if (!isfinite(z) || z <= 0)
        return 0;
    *out = z;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_RF_MICROSTRIP */
