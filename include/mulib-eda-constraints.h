/** @file mulib-eda-constraints.h
 * @brief Evaluates equality, minimum and maximum scalar constraints with absolute tolerance,
 * returning nonnegative violation magnitudes.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_CONSTRAINTS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_CONSTRAINTS_H
#define MULIB_EDA_CONSTRAINTS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_eda_constraint_relation {
    MULIB_EDA_EQ = 0,
    MULIB_EDA_LE = 1,
    MULIB_EDA_GE = 2
} mulib_eda_constraint_relation_t;
typedef struct mulib_eda_constraint {
    uint64_t object;
    int priority;
    mulib_eda_constraint_relation_t relation;
    double actual, target, tolerance;
} mulib_eda_constraint_t;
int mulib_eda_constraint_check(mulib_eda_constraint_t c, double *violation);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_CONSTRAINTS_H */
#if defined(MULIB_IMPL_EDA_CONSTRAINTS) && !defined(MULIB_EDA_CONSTRAINTS_H_IMPLEMENTED)
#define MULIB_EDA_CONSTRAINTS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_constraint_check(mulib_eda_constraint_t c, double *v) {
    if (!v || !isfinite(c.actual) || !isfinite(c.target) || !isfinite(c.tolerance) ||
        c.tolerance < 0 || c.relation < 0 || c.relation > 2)
        return 0;
    double d = c.actual - c.target;
    double excess = c.relation == MULIB_EDA_EQ
                        ? fabs(d) - c.tolerance
                        : (c.relation == MULIB_EDA_LE ? d - c.tolerance : -d - c.tolerance);
    *v = excess > 0 ? excess : 0;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_CONSTRAINTS */
