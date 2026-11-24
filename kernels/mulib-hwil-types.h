/** @file mulib-hwil-types.h
 * @brief Validation and exact matching of bit-vector and single-bit clock types.
 * @details C11 single-header implementation; define MULIB_IMPL_HWIL_TYPES in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HWIL_TYPES_H
#define MULIB_HWIL_TYPES_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_hwil_kind {
    MULIB_HWIL_BITS = 1,
    MULIB_HWIL_CLOCK = 2,
    MULIB_HWIL_MEMORY = 3
} mulib_hwil_kind_t;
typedef struct mulib_hwil_type {
    mulib_hwil_kind_t kind;
    uint32_t width;
    uint8_t signedness, fourstate;
} mulib_hwil_type_t;
int mulib_hwil_type_valid(mulib_hwil_type_t t);
int mulib_hwil_type_equal(mulib_hwil_type_t a, mulib_hwil_type_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HWIL_TYPES_H */
#if defined(MULIB_IMPL_HWIL_TYPES) && !defined(MULIB_HWIL_TYPES_H_IMPLEMENTED)
#define MULIB_HWIL_TYPES_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_type_valid(mulib_hwil_type_t t) {
    if (t.kind == MULIB_HWIL_BITS)
        return t.width > 0 && t.width <= 1048576 && t.signedness <= 1 && t.fourstate <= 1;
    if (t.kind == MULIB_HWIL_CLOCK)
        return t.width == 1 && !t.signedness && !t.fourstate;
    return 0;
}
int mulib_hwil_type_equal(mulib_hwil_type_t a, mulib_hwil_type_t b) {
    return a.kind == b.kind && a.width == b.width && a.signedness == b.signedness &&
           a.fourstate == b.fourstate;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_HWIL_TYPES */
