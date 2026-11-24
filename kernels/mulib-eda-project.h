/** @file mulib-eda-project.h
 * @brief Validated in-memory project identity, revision, variant count and reproducible seed.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_PROJECT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_PROJECT_H
#define MULIB_EDA_PROJECT_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_project {
    char name[80], revision[32];
    uint32_t board_variants;
    uint64_t seed;
} mulib_eda_project_t;
int mulib_eda_project_init(mulib_eda_project_t *p, const char *name, const char *rev,
                           uint32_t variants, uint64_t seed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_PROJECT_H */
#if defined(MULIB_IMPL_EDA_PROJECT) && !defined(MULIB_EDA_PROJECT_H_IMPLEMENTED)
#define MULIB_EDA_PROJECT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_project_init(mulib_eda_project_t *p, const char *name, const char *rev,
                           uint32_t variants, uint64_t seed) {
    if (!p || !name || !name[0] || !rev || !rev[0] || strlen(name) >= 80 || strlen(rev) >= 32 ||
        !variants)
        return 0;
    memset(p, 0, sizeof *p);
    strcpy(p->name, name);
    strcpy(p->revision, rev);
    p->board_variants = variants;
    p->seed = seed;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_PROJECT */
