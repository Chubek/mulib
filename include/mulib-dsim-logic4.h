/** @file mulib-dsim-logic4.h
 * @brief Scalar 4-state 0/1/X/Z Boolean truth tables and simple two-driver resolution without drive
 * strengths.
 * @details C11 single-header implementation; define MULIB_IMPL_DSIM_LOGIC4 in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_DSIM_LOGIC4_H
#define MULIB_DSIM_LOGIC4_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_dsim_logic4 {
    MULIB_DSIM_0 = 0,
    MULIB_DSIM_1 = 1,
    MULIB_DSIM_X = 2,
    MULIB_DSIM_Z = 3
} mulib_dsim_logic4_t;
mulib_dsim_logic4_t mulib_dsim_not(mulib_dsim_logic4_t a);
mulib_dsim_logic4_t mulib_dsim_and(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b);
mulib_dsim_logic4_t mulib_dsim_or(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b);
mulib_dsim_logic4_t mulib_dsim_xor(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b);
mulib_dsim_logic4_t mulib_dsim_resolve(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b);
char mulib_dsim_logic4_char(mulib_dsim_logic4_t v);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSIM_LOGIC4_H */
#if defined(MULIB_IMPL_DSIM_LOGIC4) && !defined(MULIB_DSIM_LOGIC4_H_IMPLEMENTED)
#define MULIB_DSIM_LOGIC4_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_dsim_logic4_t mulib_dsim_not(mulib_dsim_logic4_t a) {
    return a == MULIB_DSIM_0 ? MULIB_DSIM_1 : a == MULIB_DSIM_1 ? MULIB_DSIM_0 : MULIB_DSIM_X;
}
mulib_dsim_logic4_t mulib_dsim_and(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b) {
    if (a == MULIB_DSIM_0 || b == MULIB_DSIM_0)
        return MULIB_DSIM_0;
    if (a == MULIB_DSIM_1 && b == MULIB_DSIM_1)
        return MULIB_DSIM_1;
    return MULIB_DSIM_X;
}
mulib_dsim_logic4_t mulib_dsim_or(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b) {
    if (a == MULIB_DSIM_1 || b == MULIB_DSIM_1)
        return MULIB_DSIM_1;
    if (a == MULIB_DSIM_0 && b == MULIB_DSIM_0)
        return MULIB_DSIM_0;
    return MULIB_DSIM_X;
}
mulib_dsim_logic4_t mulib_dsim_xor(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b) {
    if (a < MULIB_DSIM_0 || b < MULIB_DSIM_0 || a >= MULIB_DSIM_X || b >= MULIB_DSIM_X)
        return MULIB_DSIM_X;
    return a == b ? MULIB_DSIM_0 : MULIB_DSIM_1;
}
mulib_dsim_logic4_t mulib_dsim_resolve(mulib_dsim_logic4_t a, mulib_dsim_logic4_t b) {
    if (a < MULIB_DSIM_0 || a > MULIB_DSIM_Z || b < MULIB_DSIM_0 || b > MULIB_DSIM_Z)
        return MULIB_DSIM_X;
    if (a == MULIB_DSIM_Z)
        return b;
    if (b == MULIB_DSIM_Z)
        return a;
    return a == b ? a : MULIB_DSIM_X;
}
char mulib_dsim_logic4_char(mulib_dsim_logic4_t v) {
    return v == 0 ? '0' : v == 1 ? '1' : v == 2 ? 'x' : v == 3 ? 'z' : '?';
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_DSIM_LOGIC4 */
