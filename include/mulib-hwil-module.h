/** @file mulib-hwil-module.h
 * @brief Append-only combinational, single-bit HWIL netlist with forward-referenced DAG disallowed
 * by construction; 512-node bound.
 * @details C11 single-header implementation; define MULIB_IMPL_HWIL_MODULE in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HWIL_MODULE_H
#define MULIB_HWIL_MODULE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-dsim-logic4.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_hwil_gate {
    MULIB_HWIL_INPUT = 0,
    MULIB_HWIL_CONST0 = 1,
    MULIB_HWIL_CONST1 = 2,
    MULIB_HWIL_NOT = 3,
    MULIB_HWIL_AND = 4,
    MULIB_HWIL_OR = 5,
    MULIB_HWIL_XOR = 6,
    MULIB_HWIL_MUX = 7
} mulib_hwil_gate_t;
typedef struct mulib_hwil_node {
    mulib_hwil_gate_t op;
    uint16_t a, b, c;
} mulib_hwil_node_t;
typedef struct mulib_hwil_module {
    size_t count;
    mulib_hwil_node_t nodes[512];
} mulib_hwil_module_t;
int mulib_hwil_module_add(mulib_hwil_module_t *m, mulib_hwil_gate_t op, uint16_t a, uint16_t b,
                          uint16_t c, uint16_t *id);
int mulib_hwil_module_eval(const mulib_hwil_module_t *m, const mulib_dsim_logic4_t *input,
                           size_t inputs, mulib_dsim_logic4_t *values, size_t capacity);
int mulib_hwil_module_valid(const mulib_hwil_module_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_HWIL_MODULE_H */
#if defined(MULIB_IMPL_HWIL_MODULE) && !defined(MULIB_HWIL_MODULE_H_IMPLEMENTED)
#define MULIB_HWIL_MODULE_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_module_add(mulib_hwil_module_t *m, mulib_hwil_gate_t op, uint16_t a, uint16_t b,
                          uint16_t c, uint16_t *id) {
    if (!m || m->count >= 512 || op < MULIB_HWIL_INPUT || op > MULIB_HWIL_MUX)
        return 0;
    size_t count = m->count;
    switch (op) {
    case MULIB_HWIL_NOT:
        if (a >= count)
            return 0;
        break;
    case MULIB_HWIL_AND:
    case MULIB_HWIL_OR:
    case MULIB_HWIL_XOR:
        if (a >= count || b >= count)
            return 0;
        break;
    case MULIB_HWIL_MUX:
        if (a >= count || b >= count || c >= count)
            return 0;
        break;
    default:
        break;
    }
    m->nodes[count] = (mulib_hwil_node_t){op, a, b, c};
    m->count++;
    if (id)
        *id = (uint16_t)count;
    return 1;
}
int mulib_hwil_module_eval(const mulib_hwil_module_t *m, const mulib_dsim_logic4_t *in, size_t ni,
                           mulib_dsim_logic4_t *v, size_t cap) {
    if (!mulib_hwil_module_valid(m) || !v || cap < m->count)
        return 0;
    size_t required = 0;
    for (size_t k = 0; k < m->count; k++)
        if (m->nodes[k].op == MULIB_HWIL_INPUT)
            required++;
    if (required > ni || (!in && required))
        return 0;
    for (size_t k = 0; k < required; k++)
        if (in[k] < MULIB_DSIM_0 || in[k] > MULIB_DSIM_Z)
            return 0;
    size_t next = 0;
    for (size_t k = 0; k < m->count; k++) {
        mulib_hwil_node_t n = m->nodes[k];
        switch (n.op) {
        case MULIB_HWIL_INPUT:
            if (next >= ni || !in || in[next] > 3)
                return 0;
            v[k] = in[next++];
            break;
        case MULIB_HWIL_CONST0:
            v[k] = MULIB_DSIM_0;
            break;
        case MULIB_HWIL_CONST1:
            v[k] = MULIB_DSIM_1;
            break;
        case MULIB_HWIL_NOT:
            v[k] = mulib_dsim_not(v[n.a]);
            break;
        case MULIB_HWIL_AND:
            v[k] = mulib_dsim_and(v[n.a], v[n.b]);
            break;
        case MULIB_HWIL_OR:
            v[k] = mulib_dsim_or(v[n.a], v[n.b]);
            break;
        case MULIB_HWIL_XOR:
            v[k] = mulib_dsim_xor(v[n.a], v[n.b]);
            break;
        case MULIB_HWIL_MUX:
            v[k] = v[n.a] == MULIB_DSIM_0   ? v[n.c]
                   : v[n.a] == MULIB_DSIM_1 ? v[n.b]
                                            : (v[n.b] == v[n.c] ? v[n.b] : MULIB_DSIM_X);
            break;
        default:
            return 0;
        }
    }
    return 1;
}
int mulib_hwil_module_valid(const mulib_hwil_module_t *m) {
    if (!m || m->count > 512)
        return 0;
    for (size_t i = 0; i < m->count; i++) {
        mulib_hwil_node_t n = m->nodes[i];
        if (n.op < MULIB_HWIL_INPUT || n.op > MULIB_HWIL_MUX)
            return 0;
        if (n.op >= MULIB_HWIL_NOT && n.a >= i)
            return 0;
        if (n.op >= MULIB_HWIL_AND && n.b >= i)
            return 0;
        if (n.op == MULIB_HWIL_MUX && n.c >= i)
            return 0;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_HWIL_MODULE */
