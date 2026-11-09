/** @file mulib-spice-independent.h
 * @brief Time-independent DC current and voltage MNA source stamps.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_INDEPENDENT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_INDEPENDENT_H
#define MULIB_SPICE_INDEPENDENT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-stamp.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_independent_current(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double value);
int mulib_spice_independent_voltage(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, size_t branch,
                                    double value);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_INDEPENDENT_H */
#if defined(MULIB_IMPL_SPICE_INDEPENDENT) && !defined(MULIB_SPICE_INDEPENDENT_H_IMPLEMENTED)
#define MULIB_SPICE_INDEPENDENT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_independent_current(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, double v) {
    return mulib_spice_stamp_current(s, a, b, v);
}
int mulib_spice_independent_voltage(mulib_spice_stamp_t *s, uint16_t a, uint16_t b, size_t branch,
                                    double v) {
    return mulib_spice_stamp_voltage(s, a, b, branch, v);
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_INDEPENDENT */
