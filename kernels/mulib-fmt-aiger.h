/** @file mulib-fmt-aiger.h
 * @brief ASCII AIGER AAG writer for a single-output combinational AIG where primary inputs occur
 * before all AND nodes; sequential and symbol extensions are excluded.
 * @details C11 single-header implementation; define MULIB_IMPL_FMT_AIGER in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FMT_AIGER_H
#define MULIB_FMT_AIGER_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-synth-aig.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fmt_aiger_write_ascii(FILE *fp, const mulib_synth_aig_t *g, mulib_synth_aig_lit_t output);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FMT_AIGER_H */
#if defined(MULIB_IMPL_FMT_AIGER) && !defined(MULIB_FMT_AIGER_H_IMPLEMENTED)
#define MULIB_FMT_AIGER_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fmt_aiger_write_ascii(FILE *fp, const mulib_synth_aig_t *g, uint16_t root) {
    if (!fp || !mulib_synth_aig_valid(g) || root / 2 > g->count || g->count > 1023)
        return 0;
    size_t inputs = 0, ands = 0;
    for (size_t i = 1; i <= g->count; i++) {
        if (g->nodes[i].input) {
            if (ands)
                return 0;
            inputs++;
        } else
            ands++;
    }
    if (fprintf(fp, "aag %zu %zu 0 1 %zu\n", g->count, inputs, ands) < 0)
        return 0;
    for (size_t i = 1; i <= inputs; i++)
        if (fprintf(fp, "%zu\n", i * 2) < 0)
            return 0;
    if (fprintf(fp, "%u\n", root) < 0)
        return 0;
    for (size_t i = inputs + 1; i <= g->count; i++)
        if (fprintf(fp, "%zu %u %u\n", i * 2, g->nodes[i].a, g->nodes[i].b) < 0)
            return 0;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FMT_AIGER */
