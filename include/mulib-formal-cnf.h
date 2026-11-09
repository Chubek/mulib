/** @file mulib-formal-cnf.h
 * @brief Bounded CNF storage for 32 variables, 512 clauses and 16 literals per clause, with
 * structural validation and an AND-gate Tseitin encoder.
 * @details C11 single-header implementation; define MULIB_IMPL_FORMAL_CNF in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_FORMAL_CNF_H
#define MULIB_FORMAL_CNF_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_FORMAL_MAX_VARS 32
#define MULIB_FORMAL_MAX_CLAUSES 512
#define MULIB_FORMAL_MAX_LITERALS 16
typedef struct mulib_formal_clause {
    int8_t literals[MULIB_FORMAL_MAX_LITERALS];
    uint8_t size;
} mulib_formal_clause_t;
typedef struct mulib_formal_cnf {
    uint8_t vars;
    size_t count;
    mulib_formal_clause_t clauses[MULIB_FORMAL_MAX_CLAUSES];
} mulib_formal_cnf_t;
int mulib_formal_cnf_add(mulib_formal_cnf_t *c, const int8_t *literals, size_t count);
int mulib_formal_cnf_validate(const mulib_formal_cnf_t *c);
int mulib_formal_cnf_and(mulib_formal_cnf_t *c, int8_t output, int8_t a, int8_t b);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_FORMAL_CNF_H */
#if defined(MULIB_IMPL_FORMAL_CNF) && !defined(MULIB_FORMAL_CNF_H_IMPLEMENTED)
#define MULIB_FORMAL_CNF_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_cnf_add(mulib_formal_cnf_t *c, const int8_t *lit, size_t n) {
    if (!c || (!lit && n) || n > MULIB_FORMAL_MAX_LITERALS || c->vars > MULIB_FORMAL_MAX_VARS ||
        c->count >= MULIB_FORMAL_MAX_CLAUSES)
        return 0;
    mulib_formal_clause_t clause = {0};
    uint8_t vars = c->vars;
    for (size_t i = 0; i < n; i++) {
        int v = lit[i];
        if (!v || v < -MULIB_FORMAL_MAX_VARS || v > MULIB_FORMAL_MAX_VARS)
            return 0;
        for (size_t k = 0; k < i; k++)
            if (lit[k] == lit[i])
                return 0;
        clause.literals[i] = lit[i];
        int a = v < 0 ? -v : v;
        if (a > vars)
            vars = (uint8_t)a;
    }
    clause.size = (uint8_t)n;
    c->clauses[c->count++] = clause;
    c->vars = vars;
    return 1;
}
int mulib_formal_cnf_validate(const mulib_formal_cnf_t *c) {
    if (!c || c->count > MULIB_FORMAL_MAX_CLAUSES || c->vars > MULIB_FORMAL_MAX_VARS)
        return 0;
    for (size_t i = 0; i < c->count; i++) {
        const mulib_formal_clause_t *p = &c->clauses[i];
        if (p->size > MULIB_FORMAL_MAX_LITERALS)
            return 0;
        for (size_t j = 0; j < p->size; j++) {
            int v = p->literals[j];
            if (!v || v < -(int)c->vars || v > c->vars)
                return 0;
        }
    }
    return 1;
}
int mulib_formal_cnf_and(mulib_formal_cnf_t *c, int8_t output, int8_t a, int8_t b) {
    if (!mulib_formal_cnf_validate(c) || c->count > MULIB_FORMAL_MAX_CLAUSES - 3 || output <= 0 ||
        output > MULIB_FORMAL_MAX_VARS || a <= 0 || a > MULIB_FORMAL_MAX_VARS || b <= 0 ||
        b > MULIB_FORMAL_MAX_VARS || a == b || output == a || output == b)
        return 0;
    int8_t x[2] = {(int8_t)-output, a}, y[2] = {(int8_t)-output, b},
           z[3] = {output, (int8_t)-a, (int8_t)-b};
    return mulib_formal_cnf_add(c, x, 2) && mulib_formal_cnf_add(c, y, 2) &&
           mulib_formal_cnf_add(c, z, 3);
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_FORMAL_CNF */
