/** @file mulib-dsim-bitvec.h
 * @brief Four-state vectors of 1..64 bits with string parsing/formatting, extraction, bitwise
 * AND/OR/XOR/NOT, zero-filled logical shifts, concatenation and modular addition.
 * @details C11 single-header implementation; define MULIB_IMPL_DSIM_BITVEC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_DSIM_BITVEC_H
#define MULIB_DSIM_BITVEC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_dsim_bitvec {
    uint64_t value, known, highz;
    uint8_t width;
} mulib_dsim_bitvec_t;
int mulib_dsim_bitvec_parse(mulib_dsim_bitvec_t *v, const char *msb_bits);
int mulib_dsim_bitvec_format(mulib_dsim_bitvec_t v, char *dst, size_t length);
mulib_dsim_logic4_t mulib_dsim_bitvec_bit(mulib_dsim_bitvec_t v, unsigned index);
int mulib_dsim_bitvec_and(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
/* Operations preserve width; arithmetic turns the entire result X if any bit is X/Z.
 * Shift vacated bits are known zero. Concatenation places a above b. */
int mulib_dsim_bitvec_or(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_xor(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_not(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_add(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_shift(mulib_dsim_bitvec_t a, unsigned amount, int right,
                            mulib_dsim_bitvec_t *out);
int mulib_dsim_bitvec_concat(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b,
                             mulib_dsim_bitvec_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_DSIM_BITVEC_H */
#if defined(MULIB_IMPL_DSIM_BITVEC) && !defined(MULIB_DSIM_BITVEC_H_IMPLEMENTED)
#define MULIB_DSIM_BITVEC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
mulib_dsim_logic4_t mulib_dsim_bitvec_bit(mulib_dsim_bitvec_t v, unsigned i) {
    if (v.width > 64 || i >= 64 || i >= v.width)
        return MULIB_DSIM_X;
    uint64_t m = UINT64_C(1) << i;
    if (v.highz & m)
        return MULIB_DSIM_Z;
    if (!(v.known & m))
        return MULIB_DSIM_X;
    return v.value & m ? MULIB_DSIM_1 : MULIB_DSIM_0;
}
int mulib_dsim_bitvec_parse(mulib_dsim_bitvec_t *v, const char *s) {
    if (!v || !s)
        return 0;
    size_t n = strlen(s);
    if (!n || n > 64)
        return 0;
    mulib_dsim_bitvec_t x = {0};
    x.width = (uint8_t)n;
    for (size_t i = 0; i < n; i++) {
        char c = s[n - 1 - i];
        uint64_t m = UINT64_C(1) << i;
        if (c == '1') {
            x.value |= m;
            x.known |= m;
        } else if (c == '0')
            x.known |= m;
        else if (c == 'z' || c == 'Z')
            x.highz |= m;
        else if (c != 'x' && c != 'X')
            return 0;
    }
    *v = x;
    return 1;
}
int mulib_dsim_bitvec_format(mulib_dsim_bitvec_t v, char *dst, size_t n) {
    if (!dst || !v.width || v.width > 64 || n <= (size_t)v.width)
        return 0;
    for (unsigned i = 0; i < v.width; i++)
        dst[i] = mulib_dsim_logic4_char(mulib_dsim_bitvec_bit(v, v.width - i - 1));
    dst[v.width] = 0;
    return 1;
}
int mulib_dsim_bitvec_and(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out) {
    if (!out || !a.width || a.width > 64 || a.width != b.width)
        return 0;
    mulib_dsim_bitvec_t r = {0};
    r.width = a.width;
    for (unsigned i = 0; i < r.width; i++) {
        mulib_dsim_logic4_t bit =
            mulib_dsim_and(mulib_dsim_bitvec_bit(a, i), mulib_dsim_bitvec_bit(b, i));
        uint64_t m = UINT64_C(1) << i;
        if (bit <= MULIB_DSIM_1) {
            r.known |= m;
            if (bit == MULIB_DSIM_1)
                r.value |= m;
        }
    }
    *out = r;
    return 1;
}
static uint64_t mulib_dsim_bitvec_mask_(unsigned width) {
    return width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
}
static int mulib_dsim_bitvec_binary_(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b,
                                     mulib_dsim_bitvec_t *out, int xor_op) {
    if (!out || !a.width || a.width > 64 || a.width != b.width)
        return 0;
    mulib_dsim_bitvec_t r = {0};
    r.width = a.width;
    for (unsigned i = 0; i < a.width; i++) {
        mulib_dsim_logic4_t x = mulib_dsim_bitvec_bit(a, i), y = mulib_dsim_bitvec_bit(b, i);
        mulib_dsim_logic4_t z = xor_op ? mulib_dsim_xor(x, y) : mulib_dsim_or(x, y);
        uint64_t m = UINT64_C(1) << i;
        if (z == MULIB_DSIM_0 || z == MULIB_DSIM_1)
            r.known |= m;
        if (z == MULIB_DSIM_1)
            r.value |= m;
    }
    *out = r;
    return 1;
}
int mulib_dsim_bitvec_or(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out) {
    return mulib_dsim_bitvec_binary_(a, b, out, 0);
}
int mulib_dsim_bitvec_xor(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out) {
    return mulib_dsim_bitvec_binary_(a, b, out, 1);
}
int mulib_dsim_bitvec_not(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t *out) {
    if (!out || !a.width || a.width > 64)
        return 0;
    uint64_t mask = mulib_dsim_bitvec_mask_(a.width);
    a.known &= mask & ~a.highz;
    a.value = ~a.value & a.known;
    a.highz = 0;
    *out = a;
    return 1;
}
int mulib_dsim_bitvec_add(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b, mulib_dsim_bitvec_t *out) {
    if (!out || !a.width || a.width > 64 || a.width != b.width)
        return 0;
    uint64_t mask = mulib_dsim_bitvec_mask_(a.width);
    mulib_dsim_bitvec_t r = {0};
    r.width = a.width;
    if (((a.known & ~a.highz) & mask) == mask && ((b.known & ~b.highz) & mask) == mask) {
        r.value = (a.value + b.value) & mask;
        r.known = mask;
    }
    *out = r;
    return 1;
}
int mulib_dsim_bitvec_shift(mulib_dsim_bitvec_t a, unsigned n, int right,
                            mulib_dsim_bitvec_t *out) {
    if (!out || !a.width || a.width > 64 || (right != 0 && right != 1))
        return 0;
    uint64_t mask = mulib_dsim_bitvec_mask_(a.width);
    if (n >= a.width) {
        a.value = a.highz = 0;
        a.known = mask;
    } else if (n) {
        if (right) {
            a.value >>= n;
            a.highz >>= n;
            a.known = (a.known >> n) | (mask ^ (mask >> n));
        } else {
            a.value = (a.value << n) & mask;
            a.highz = (a.highz << n) & mask;
            a.known = ((a.known << n) | ((UINT64_C(1) << n) - 1)) & mask;
        }
    }
    a.highz &= mask;
    a.known &= mask & ~a.highz;
    a.value &= a.known;
    *out = a;
    return 1;
}
int mulib_dsim_bitvec_concat(mulib_dsim_bitvec_t a, mulib_dsim_bitvec_t b,
                             mulib_dsim_bitvec_t *out) {
    if (!out || !a.width || !b.width || a.width > 64 || b.width > 64 ||
        (unsigned)a.width + b.width > 64)
        return 0;
    uint64_t am = mulib_dsim_bitvec_mask_(a.width), bm = mulib_dsim_bitvec_mask_(b.width);
    mulib_dsim_bitvec_t r = {0};
    r.width = (uint8_t)(a.width + b.width);
    r.highz = ((a.highz & am) << b.width) | (b.highz & bm);
    r.known = (((a.known & am) << b.width) | (b.known & bm)) & ~r.highz;
    r.value = (((a.value & am) << b.width) | (b.value & bm)) & r.known;
    *out = r;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_DSIM_BITVEC */
