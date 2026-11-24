/** @file mulib-formal-cec.h
 * @brief Exact combinational equivalence for explicit truth tables with at most six inputs.
 * @details C11 single-header implementation; define MULIB_IMPL_FORMAL_CEC in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FORMAL_CEC_H
#define MULIB_FORMAL_CEC_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-synth-truthtable.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_truth_equivalent(mulib_synth_truth_t a, mulib_synth_truth_t b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FORMAL_CEC_H */
#if defined(MULIB_IMPL_FORMAL_CEC) && !defined(MULIB_FORMAL_CEC_H_IMPLEMENTED)
#define MULIB_FORMAL_CEC_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_truth_equivalent(mulib_synth_truth_t a, mulib_synth_truth_t b) {
    if (!mulib_synth_truth_valid(a) || !mulib_synth_truth_valid(b) || a.variables != b.variables)
        return 0;
    uint64_t mask = a.variables == 6 ? UINT64_MAX : ((UINT64_C(1) << (1u << a.variables)) - 1);
    return ((a.bits ^ b.bits) & mask) == 0;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FORMAL_CEC */
