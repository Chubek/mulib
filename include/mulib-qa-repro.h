/** @file mulib-qa-repro.h
 * @brief Platform-independent FNV-1a fingerprint for a reproducibility record (seed, feature mask,
 * compiler and target strings).
 * @details C11 single-header implementation; define MULIB_IMPL_QA_REPRO in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_QA_REPRO_H
#define MULIB_QA_REPRO_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_qa_repro {
    uint64_t seed;
    uint32_t version, features;
    char compiler[64], target[64];
} mulib_qa_repro_t;
int mulib_qa_repro_init(mulib_qa_repro_t *r, uint64_t seed, uint32_t features, const char *compiler,
                        const char *target);
uint64_t mulib_qa_repro_hash(const mulib_qa_repro_t *r);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_QA_REPRO_H */
#if defined(MULIB_IMPL_QA_REPRO) && !defined(MULIB_QA_REPRO_H_IMPLEMENTED)
#define MULIB_QA_REPRO_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_qa_repro_init(mulib_qa_repro_t *r, uint64_t seed, uint32_t f, const char *comp,
                        const char *target) {
    if (!r || !comp || !target || strlen(comp) >= 64 || strlen(target) >= 64)
        return 0;
    memset(r, 0, sizeof *r);
    r->seed = seed;
    r->version = 1;
    r->features = f;
    strcpy(r->compiler, comp);
    strcpy(r->target, target);
    return 1;
}
static uint64_t mulib_qa_fnv_(uint64_t h, uint8_t byte) {
    return (h ^ byte) * UINT64_C(1099511628211);
}
uint64_t mulib_qa_repro_hash(const mulib_qa_repro_t *r) {
    if (!r || !memchr(r->compiler, 0, sizeof r->compiler) ||
        !memchr(r->target, 0, sizeof r->target))
        return 0;
    uint64_t h = UINT64_C(14695981039346656037),
             v[2] = {r->seed, ((uint64_t)r->version << 32) | r->features};
    for (size_t i = 0; i < 2; i++)
        for (unsigned j = 0; j < 8; j++)
            h = mulib_qa_fnv_(h, (uint8_t)(v[i] >> (8 * j)));
    for (size_t i = 0; i < strlen(r->compiler); i++)
        h = mulib_qa_fnv_(h, (uint8_t)r->compiler[i]);
    h = mulib_qa_fnv_(h, 0);
    for (size_t i = 0; i < strlen(r->target); i++)
        h = mulib_qa_fnv_(h, (uint8_t)r->target[i]);
    return h;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_QA_REPRO */
