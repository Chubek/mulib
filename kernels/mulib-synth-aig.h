/** @file mulib-synth-aig.h
 * @brief Bounded AIG with constant 0/1, complemented edges, structural hashing and simple
 * constant/identity simplification.
 * @details C11 single-header implementation; define MULIB_IMPL_SYNTH_AIG in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SYNTH_AIG_H
#define MULIB_SYNTH_AIG_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uint16_t mulib_synth_aig_lit_t;
typedef struct mulib_synth_aig_node {
    uint16_t a, b;
    uint8_t input;
} mulib_synth_aig_node_t;
typedef struct mulib_synth_aig {
    size_t count, inputs;
    mulib_synth_aig_node_t nodes[1024];
} mulib_synth_aig_t;
int mulib_synth_aig_input(mulib_synth_aig_t *g, mulib_synth_aig_lit_t *out);
int mulib_synth_aig_and(mulib_synth_aig_t *g, mulib_synth_aig_lit_t a, mulib_synth_aig_lit_t b,
                        mulib_synth_aig_lit_t *out);
int mulib_synth_aig_eval(const mulib_synth_aig_t *g, const uint8_t *inputs, size_t count,
                         mulib_synth_aig_lit_t root, int *out);
int mulib_synth_aig_valid(const mulib_synth_aig_t *g);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SYNTH_AIG_H */
#if defined(MULIB_IMPL_SYNTH_AIG) && !defined(MULIB_SYNTH_AIG_H_IMPLEMENTED)
#define MULIB_SYNTH_AIG_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_aig_input(mulib_synth_aig_t *g, mulib_synth_aig_lit_t *out) {
    if (!g || !out || g->count >= 1023)
        return 0;
    size_t i = ++g->count;
    g->nodes[i] = (mulib_synth_aig_node_t){0, 0, 1};
    g->inputs++;
    *out = (uint16_t)(2 * i);
    return 1;
}
int mulib_synth_aig_and(mulib_synth_aig_t *g, uint16_t a, uint16_t b, uint16_t *out) {
    if (!g || g->count > 1023 || !out || a / 2 > g->count || b / 2 > g->count)
        return 0;
    if (a == 0 || b == 0) {
        *out = 0;
        return 1;
    }
    if (a == 1) {
        *out = b;
        return 1;
    }
    if (b == 1 || a == b) {
        *out = a;
        return 1;
    }
    if (a == (uint16_t)(b ^ 1)) {
        *out = 0;
        return 1;
    }
    if (a > b) {
        uint16_t t = a;
        a = b;
        b = t;
    }
    for (size_t i = 1; i <= g->count; i++)
        if (!g->nodes[i].input && g->nodes[i].a == a && g->nodes[i].b == b) {
            *out = (uint16_t)(i * 2);
            return 1;
        }
    if (g->count >= 1023)
        return 0;
    size_t i = ++g->count;
    g->nodes[i] = (mulib_synth_aig_node_t){a, b, 0};
    *out = (uint16_t)(i * 2);
    return 1;
}
int mulib_synth_aig_eval(const mulib_synth_aig_t *g, const uint8_t *input, size_t n, uint16_t root,
                         int *out) {
    if (!mulib_synth_aig_valid(g) || !out || root / 2 > g->count || n < g->inputs ||
        (!input && g->inputs))
        return 0;
    uint8_t values[1024] = {0};
    size_t ii = 0;
    for (size_t i = 1; i <= g->count; i++) {
        mulib_synth_aig_node_t node = g->nodes[i];
        if (node.input) {
            if (input[ii] > 1)
                return 0;
            values[i] = input[ii++];
        } else
            values[i] =
                (values[node.a / 2] ^ ((node.a) & 1)) & (values[node.b / 2] ^ ((node.b) & 1));
    }
    *out = (values[root / 2] ^ (root & 1));
    return 1;
}
int mulib_synth_aig_valid(const mulib_synth_aig_t *g) {
    if (!g || g->count > 1023)
        return 0;
    size_t inputs = 0;
    for (size_t i = 1; i <= g->count; i++) {
        mulib_synth_aig_node_t n = g->nodes[i];
        if (n.input > 1)
            return 0;
        if (n.input)
            inputs++;
        else if (n.a / 2 >= i || n.b / 2 >= i)
            return 0;
    }
    return inputs == g->inputs;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SYNTH_AIG */
