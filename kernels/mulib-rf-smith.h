/** @file mulib-rf-smith.h
 * @brief Smith chart Cartesian coordinates from normalized single-port impedance using the S11
 * transform.
 * @details C11 single-header implementation; define MULIB_IMPL_RF_SMITH in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_RF_SMITH_H
#define MULIB_RF_SMITH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-rf-sparams.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_smith_xy(mulib_rf_complex_t impedance, double z0, double *x, double *y);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RF_SMITH_H */
#if defined(MULIB_IMPL_RF_SMITH) && !defined(MULIB_RF_SMITH_H_IMPLEMENTED)
#define MULIB_RF_SMITH_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_rf_smith_xy(mulib_rf_complex_t z, double z0, double *x, double *y) {
    if (!x || !y)
        return 0;
    mulib_rf_complex_t g;
    if (!mulib_rf_s11_from_impedance(z, z0, &g))
        return 0;
    *x = g.re;
    *y = g.im;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_RF_SMITH */
