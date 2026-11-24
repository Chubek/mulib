/** @file mulib-spice-r.h
 * @brief Linear DC resistor MNA stamp with strictly positive finite resistance.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_R in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_R_H
#define MULIB_SPICE_R_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-stamp.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_r_stamp(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double resistance);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_R_H */
#if defined(MULIB_IMPL_SPICE_R) && !defined(MULIB_SPICE_R_H_IMPLEMENTED)
#define MULIB_SPICE_R_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_r_stamp(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double r) {
    return isfinite(r) && r > 0 && mulib_spice_stamp_conductance(s, a, b, 1 / r);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_R */
