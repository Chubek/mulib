/** @file mulib-qa-corpus.h
 * @brief Bounded 256-entry registry of golden fixture identifiers, input fingerprints and expected
 * statuses.
 * @details C11 single-header implementation; define MULIB_IMPL_QA_CORPUS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_QA_CORPUS_H
#define MULIB_QA_CORPUS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_qa_case {
    char id[48];
    uint64_t input_fingerprint;
    int expected_status;
} mulib_qa_case_t;
typedef struct mulib_qa_corpus {
    mulib_qa_case_t cases[256];
    size_t count;
} mulib_qa_corpus_t;
int mulib_qa_corpus_add(mulib_qa_corpus_t *c, mulib_qa_case_t item);
const mulib_qa_case_t *mulib_qa_corpus_find(const mulib_qa_corpus_t *c, const char *id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_QA_CORPUS_H */
#if defined(MULIB_IMPL_QA_CORPUS) && !defined(MULIB_QA_CORPUS_H_IMPLEMENTED)
#define MULIB_QA_CORPUS_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
const mulib_qa_case_t *mulib_qa_corpus_find(const mulib_qa_corpus_t *c, const char *id) {
    if (!c || c->count > 256 || !id)
        return NULL;
    for (size_t i = 0; i < c->count; i++)
        if (!strcmp(id, c->cases[i].id))
            return c->cases + i;
    return NULL;
}
int mulib_qa_corpus_add(mulib_qa_corpus_t *c, mulib_qa_case_t x) {
    if (!c || !x.id[0] || !memchr(x.id, 0, sizeof x.id) || c->count >= 256 ||
        mulib_qa_corpus_find(c, x.id))
        return 0;
    c->cases[c->count++] = x;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_QA_CORPUS */
