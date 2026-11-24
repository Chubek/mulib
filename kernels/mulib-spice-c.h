/** @file mulib-spice-c.h
 * @brief Backward-Euler companion for an ideal linear capacitor: conductance C/dt and equivalent
 * history current.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_C in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_C_H
#define MULIB_SPICE_C_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-stamp.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_c_backward_euler(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double capacitance,
                                 double dt, double previous_va, double previous_vb);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_C_H */
#if defined(MULIB_IMPL_SPICE_C) && !defined(MULIB_SPICE_C_H_IMPLEMENTED)
#define MULIB_SPICE_C_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_c_backward_euler(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double c,
                                 double dt, double va, double vb) {
    if (!isfinite(c) || c < 0 || !isfinite(dt) || dt <= 0 || !isfinite(va) || !isfinite(vb))
        return 0;
    double g = c / dt;
    if (!isfinite(g) || !isfinite(-g * (va - vb)) || !mulib_spice_stamp_conductance(s, a, b, g))
        return 0;
    return mulib_spice_stamp_current(s, a, b, -g * (va - vb));
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_C */
