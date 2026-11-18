/** @file mulib-cdd-query.h
 * @brief Typed predicates, capability filters and deterministic cost ranking.
 * ISO C11; define MULIB_IMPL_CDD_QUERY in exactly one translation unit.
 * See manual/mulib-cdd-query.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_QUERY_H
#define MULIB_CDD_QUERY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-catalogue.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { MULIB_CDD_EQ=1,MULIB_CDD_NE,MULIB_CDD_LT,MULIB_CDD_LE,MULIB_CDD_GT,MULIB_CDD_GE,MULIB_CDD_EXISTS } mulib_cdd_op_t;
typedef struct { const char *key; mulib_cdd_op_t op; mulib_cdd_value_t value; } mulib_cdd_pred_t;
typedef struct { const char *category; const mulib_cdd_req_t *caps; size_t cap_count;
    const mulib_cdd_pred_t *preds; size_t pred_count; } mulib_cdd_query_t;
int mulib_cdd_query_valid(const mulib_cdd_query_t *q);
int mulib_cdd_query_matches(const mulib_cdd_entry_t *e,const mulib_cdd_query_t *q);
/* Writes up to capacity sorted indices. LIMIT with nonzero needed means
 * truncation. On argument or catalogue validation failure, needed is zero
 * and out is untouched. */
mulib_cdd_status_t mulib_cdd_query_run(const mulib_cdd_catalogue_t *c,const mulib_cdd_query_t *q,size_t *out,size_t capacity,size_t *needed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_QUERY_H */
#if defined(MULIB_IMPL_CDD_QUERY) && !defined(MULIB_CDD_QUERY_IMPLEMENTED)
#define MULIB_CDD_QUERY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_cdd_query_valid(const mulib_cdd_query_t *q) {
    if (!q || (q->category && !mulib_cdd_name_valid(q->category)) || (!q->caps && q->cap_count) || (!q->preds && q->pred_count)) return 0;
    for (size_t i=0;i<q->cap_count;i++) if (!mulib_cdd_req_valid(q->caps+i)) return 0;
    for (size_t i=0;i<q->pred_count;i++) {
        const mulib_cdd_pred_t *p=q->preds+i;
        if (!mulib_cdd_name_valid(p->key) || p->op<MULIB_CDD_EQ || p->op>MULIB_CDD_EXISTS ||
            (p->op!=MULIB_CDD_EXISTS && !mulib_cdd_value_valid(&p->value)) ||
            (p->op>=MULIB_CDD_LT && p->op<=MULIB_CDD_GE && p->value.kind!=MULIB_CDD_INT)) return 0;
    }
    return 1;
}
int mulib_cdd_query_matches(const mulib_cdd_entry_t *e,const mulib_cdd_query_t *q) {
    if (!e || !mulib_cdd_query_valid(q) || (q->category && strcmp(e->category,q->category))) return 0;
    for (size_t i=0;i<q->cap_count;i++) if (!mulib_cdd_entry_provides(e,q->caps+i)) return 0;
    for (size_t i=0;i<q->pred_count;i++) {
        const mulib_cdd_pred_t *p=q->preds+i;
        const mulib_cdd_attr_t *a=mulib_cdd_attr_get(e,p->key);
        if (!a) return 0;
        if (p->op==MULIB_CDD_EXISTS) continue;
        if (a->value.kind!=p->value.kind) return 0;
        int cmp=a->value.kind==MULIB_CDD_STRING?strcmp(a->value.string,p->value.string):
          (a->value.integer>p->value.integer)-(a->value.integer<p->value.integer);
        switch (p->op) {
        case MULIB_CDD_EQ: if (cmp!=0) return 0; break;
        case MULIB_CDD_NE: if (cmp==0) return 0; break;
        case MULIB_CDD_LT: if (cmp>=0) return 0; break;
        case MULIB_CDD_LE: if (cmp>0) return 0; break;
        case MULIB_CDD_GT: if (cmp<=0) return 0; break;
        case MULIB_CDD_GE: if (cmp<0) return 0; break;
        default: return 0;
        }
    }
    return 1;
}
mulib_cdd_status_t mulib_cdd_query_run(const mulib_cdd_catalogue_t *c,const mulib_cdd_query_t *q,size_t *out,size_t capacity,size_t *needed) {
    if (!needed) return MULIB_CDD_INVALID;
    *needed=0;
    if ((!out && capacity) || !mulib_cdd_query_valid(q)) return MULIB_CDD_INVALID;
    mulib_cdd_status_t s=mulib_cdd_validate(c,NULL); if (s) return s;
    size_t sorted[MULIB_CDD_MAX_ENTRIES],n=0;
    for (size_t i=0;i<c->count;i++) if (mulib_cdd_query_matches(c->entries+i,q)) {
        size_t j=n;
        while (j) {
            const mulib_cdd_entry_t *a=c->entries+sorted[j-1],*b=c->entries+i;
            if (a->cost<b->cost || (a->cost==b->cost && strcmp(a->id,b->id)<0)) break;
            sorted[j]=sorted[j-1]; j--;
        }
        sorted[j]=i; n++;
    }
    *needed=n;
    for (size_t i=0;i<n && i<capacity;i++) out[i]=sorted[i];
    return n>capacity?MULIB_CDD_LIMIT:MULIB_CDD_OK;
}
#ifdef __cplusplus
}
#endif
#endif
