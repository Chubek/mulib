/** @file mulib-fmt-sv.h
 * @brief Structural synthesizable Verilog module emitter for one-bit HWIL combinational nodes, one
 * output and generated port names.
 * @details C11 single-header implementation; define MULIB_IMPL_FMT_SV in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FMT_SV_H
#define MULIB_FMT_SV_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "mulib-hwil-module.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fmt_sv_emit(FILE *out, const mulib_hwil_module_t *module, size_t output_node);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FMT_SV_H */
#if defined(MULIB_IMPL_FMT_SV) && !defined(MULIB_FMT_SV_H_IMPLEMENTED)
#define MULIB_FMT_SV_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fmt_sv_emit(FILE *out, const mulib_hwil_module_t *m, size_t root) {
    if (!out || !mulib_hwil_module_valid(m) || root >= m->count)
        return 0;
    size_t inputs = 0;
    for (size_t i = 0; i < m->count; i++)
        if (m->nodes[i].op == MULIB_HWIL_INPUT)
            inputs++;
    if (fputs("module amb3r_generated (\n", out) < 0)
        return 0;
    for (size_t k = 0; k < inputs; k++)
        if (fprintf(out, "  input wire in%zu,\n", k) < 0)
            return 0;
    if (fputs("  output wire out\n);\n", out) < 0)
        return 0;
    size_t in = 0;
    for (size_t i = 0; i < m->count; i++) {
        mulib_hwil_node_t g = m->nodes[i];
        if (g.op == MULIB_HWIL_INPUT) {
            if (fprintf(out, "wire n%zu = in%zu;\n", i, in++) < 0)
                return 0;
            continue;
        }
        char expr[120];
        switch (g.op) {
        case MULIB_HWIL_CONST0:
            snprintf(expr, sizeof expr, "1'b0");
            break;
        case MULIB_HWIL_CONST1:
            snprintf(expr, sizeof expr, "1'b1");
            break;
        case MULIB_HWIL_NOT:
            snprintf(expr, sizeof expr, "~n%u", g.a);
            break;
        case MULIB_HWIL_AND:
            snprintf(expr, sizeof expr, "n%u & n%u", g.a, g.b);
            break;
        case MULIB_HWIL_OR:
            snprintf(expr, sizeof expr, "n%u | n%u", g.a, g.b);
            break;
        case MULIB_HWIL_XOR:
            snprintf(expr, sizeof expr, "n%u ^ n%u", g.a, g.b);
            break;
        case MULIB_HWIL_MUX:
            snprintf(expr, sizeof expr, "n%u ? n%u : n%u", g.a, g.b, g.c);
            break;
        default:
            return 0;
        }
        if (fprintf(out, "wire n%zu = %s;\n", i, expr) < 0)
            return 0;
    }
    return fprintf(out, "assign out = n%zu;\nendmodule\n", root) >= 0;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FMT_SV */
