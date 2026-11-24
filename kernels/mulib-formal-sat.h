/** @file mulib-formal-sat.h
 * @brief Correct bounded DPLL SAT solver with repeated unit propagation, chronological branching
 * and branch-budget UNKNOWN.
 * @details C11 single-header implementation; define MULIB_IMPL_FORMAL_SAT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FORMAL_SAT_H
#define MULIB_FORMAL_SAT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-formal-cnf.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_formal_sat_result {
    MULIB_FORMAL_SAT_UNKNOWN = -1,
    MULIB_FORMAL_SAT_UNSAT = 0,
    MULIB_FORMAL_SAT_SAT = 1
} mulib_formal_sat_result_t;
mulib_formal_sat_result_t mulib_formal_sat_solve(const mulib_formal_cnf_t *cnf,
                                                 int8_t assignment[33], size_t max_branches);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FORMAL_SAT_H */
#if defined(MULIB_IMPL_FORMAL_SAT) && !defined(MULIB_FORMAL_SAT_H_IMPLEMENTED)
#define MULIB_FORMAL_SAT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_formal_propagate_(const mulib_formal_cnf_t *c, int8_t v[33]) {
    int changed;
    do {
        changed = 0;
        for (size_t i = 0; i < c->count; i++) {
            const mulib_formal_clause_t *cl = &c->clauses[i];
            size_t un = 0;
            int last = 0, satisfied = 0;
            for (size_t k = 0; k < cl->size; k++) {
                int l = cl->literals[k], idx = l < 0 ? -l : l, sgn = l > 0 ? 1 : -1;
                if (v[idx] == sgn) {
                    satisfied = 1;
                    break;
                }
                if (v[idx] == 0) {
                    un++;
                    last = l;
                }
            }
            if (satisfied)
                continue;
            if (!un)
                return 0;
            if (un == 1) {
                int idx = last < 0 ? -last : last, sgn = last > 0 ? 1 : -1;
                v[idx] = (int8_t)sgn;
                changed = 1;
            }
        }
    } while (changed);
    return 1;
}
static int mulib_formal_dpll_(const mulib_formal_cnf_t *c, int8_t v[33], size_t *budget) {
    if (!*budget)
        return -1;
    --(*budget);
    if (!mulib_formal_propagate_(c, v))
        return 0;
    int next = 0;
    for (int i = 1; i <= c->vars; i++)
        if (v[i] == 0) {
            next = i;
            break;
        }
    if (!next)
        return 1;
    int unknown = 0;
    for (int sign = 1; sign >= -1; sign -= 2) {
        int8_t copy[33];
        memcpy(copy, v, 33);
        copy[next] = (int8_t)sign;
        int res = mulib_formal_dpll_(c, copy, budget);
        if (res == 1) {
            memcpy(v, copy, 33);
            return 1;
        }
        if (res < 0)
            unknown = 1;
    }
    return unknown ? -1 : 0;
}
mulib_formal_sat_result_t mulib_formal_sat_solve(const mulib_formal_cnf_t *c, int8_t assignment[33],
                                                 size_t budget) {
    if (!mulib_formal_cnf_validate(c) || !assignment || !budget || c->vars > 32)
        return MULIB_FORMAL_SAT_UNKNOWN;
    int8_t v[33] = {0};
    int r = mulib_formal_dpll_(c, v, &budget);
    if (r == 1)
        memcpy(assignment, v, 33);
    return (mulib_formal_sat_result_t)r;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FORMAL_SAT */
