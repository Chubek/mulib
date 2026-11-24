/** @file mulib-hwil-verify.h
 * @brief Deterministic structural verification for bounded combinational HWIL modules; checks
 * opcode validity, input references and topological order.
 * @details C11 single-header implementation; define MULIB_IMPL_HWIL_VERIFY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HWIL_VERIFY_H
#define MULIB_HWIL_VERIFY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-hwil-module.h"
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_module_verify(const mulib_hwil_module_t *m, size_t *invalid_node);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HWIL_VERIFY_H */
#if defined(MULIB_IMPL_HWIL_VERIFY) && !defined(MULIB_HWIL_VERIFY_H_IMPLEMENTED)
#define MULIB_HWIL_VERIFY_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_module_verify(const mulib_hwil_module_t *m, size_t *bad) {
    if (!m || m->count > 512)
        return 0;
    for (size_t i = 0; i < m->count; i++) {
        mulib_hwil_node_t n = m->nodes[i];
        int ok = 1;
        if (n.op < MULIB_HWIL_INPUT || n.op > MULIB_HWIL_MUX)
            ok = 0;
        else if (n.op == MULIB_HWIL_NOT)
            ok = n.a < i;
        else if (n.op >= MULIB_HWIL_AND && n.op <= MULIB_HWIL_XOR)
            ok = n.a < i && n.b < i;
        else if (n.op == MULIB_HWIL_MUX)
            ok = n.a < i && n.b < i && n.c < i;
        if (!ok) {
            if (bad)
                *bad = i;
            return 0;
        }
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_HWIL_VERIFY */
