/** @file mulib-synth-truthtable.h
 * @brief Packed truth tables for zero to six inputs with variable generation, Boolean operations,
 * evaluation and variable-removing cofactors.
 * @details C11 single-header implementation; define MULIB_IMPL_SYNTH_TRUTHTABLE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SYNTH_TRUTHTABLE_H
#define MULIB_SYNTH_TRUTHTABLE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_synth_truth {
    uint8_t variables;
    uint64_t bits;
} mulib_synth_truth_t;
int mulib_synth_truth_valid(mulib_synth_truth_t t);
int mulib_synth_truth_variable(unsigned vars, unsigned index, mulib_synth_truth_t *out);
int mulib_synth_truth_apply(mulib_synth_truth_t a, mulib_synth_truth_t b, char operation,
                            mulib_synth_truth_t *out);
int mulib_synth_truth_eval(mulib_synth_truth_t t, uint8_t inputs, int *out);
int mulib_synth_truth_cofactor(mulib_synth_truth_t t, unsigned variable, int value,
                               mulib_synth_truth_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SYNTH_TRUTHTABLE_H */
#if defined(MULIB_IMPL_SYNTH_TRUTHTABLE) && !defined(MULIB_SYNTH_TRUTHTABLE_H_IMPLEMENTED)
#define MULIB_SYNTH_TRUTHTABLE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_truth_valid(mulib_synth_truth_t t) { return t.variables <= 6; }
int mulib_synth_truth_variable(unsigned vars, unsigned idx, mulib_synth_truth_t *out) {
    if (!out || vars > 6 || idx >= vars)
        return 0;
    mulib_synth_truth_t v = {(uint8_t)vars, 0};
    for (unsigned x = 0; x < (1u << vars); x++)
        if (x & (1u << idx))
            v.bits |= UINT64_C(1) << x;
    *out = v;
    return 1;
}
int mulib_synth_truth_apply(mulib_synth_truth_t a, mulib_synth_truth_t b, char op,
                            mulib_synth_truth_t *out) {
    if (!out || !mulib_synth_truth_valid(a) || a.variables != b.variables)
        return 0;
    mulib_synth_truth_t v = a;
    switch (op) {
    case '&':
        v.bits = a.bits & b.bits;
        break;
    case '|':
        v.bits = a.bits | b.bits;
        break;
    case '^':
        v.bits = a.bits ^ b.bits;
        break;
    default:
        return 0;
    }
    if (v.variables < 6)
        v.bits &= (UINT64_C(1) << (1u << v.variables)) - 1;
    *out = v;
    return 1;
}
int mulib_synth_truth_eval(mulib_synth_truth_t t, uint8_t inputs, int *out) {
    if (!out || t.variables > 6 || inputs >= (1u << t.variables))
        return 0;
    *out = (int)((t.bits >> inputs) & 1);
    return 1;
}
int mulib_synth_truth_cofactor(mulib_synth_truth_t t, unsigned variable, int value,
                               mulib_synth_truth_t *out) {
    if (!out || t.variables > 6 || variable >= t.variables || (value != 0 && value != 1))
        return 0;
    mulib_synth_truth_t r = {(uint8_t)(t.variables - 1), 0};
    unsigned low = (1u << variable) - 1;
    for (unsigned i = 0; i < (1u << r.variables); i++) {
        unsigned src = (i & low) | ((unsigned)value << variable) | ((i & ~low) << 1);
        if ((t.bits >> src) & 1)
            r.bits |= UINT64_C(1) << i;
    }
    *out = r;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SYNTH_TRUTHTABLE */
