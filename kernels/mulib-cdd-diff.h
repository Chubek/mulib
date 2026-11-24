/** @file mulib-cdd-diff.h
 * @brief Structural catalogue deltas by stable identity, independent of entry ordering.
 * ISO C11; define MULIB_IMPL_CDD_DIFF in exactly one translation unit.
 * See manual/mulib-cdd-diff.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_DIFF_H
#define MULIB_CDD_DIFF_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-catalogue.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { MULIB_CDD_ADDED=1,MULIB_CDD_REMOVED,MULIB_CDD_CHANGED } mulib_cdd_change_kind_t;
typedef struct { mulib_cdd_change_kind_t kind; size_t before,after; } mulib_cdd_change_t;
/* Metadata arrays are compared in declared order. Reports removals/changes in
 * old order, then additions in new order. LIMIT with nonzero needed means
 * truncation. On argument or catalogue validation failure, needed is zero
 * and out is untouched. */
mulib_cdd_status_t mulib_cdd_diff(const mulib_cdd_catalogue_t *before,const mulib_cdd_catalogue_t *after,mulib_cdd_change_t *out,size_t capacity,size_t *needed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_DIFF_H */
#if defined(MULIB_IMPL_CDD_DIFF) && !defined(MULIB_CDD_DIFF_IMPLEMENTED)
#define MULIB_CDD_DIFF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_cdd_diff_equal_(const mulib_cdd_entry_t *a,const mulib_cdd_entry_t *b) {
    if (strcmp(a->category,b->category) || strcmp(a->implementation,b->implementation) || a->revision!=b->revision || a->cost!=b->cost ||
        a->provide_count!=b->provide_count || a->require_count!=b->require_count || a->conflict_count!=b->conflict_count || a->attr_count!=b->attr_count) return 0;
    for (size_t i=0;i<a->provide_count;i++) if (strcmp(a->provides[i].name,b->provides[i].name) || a->provides[i].revision!=b->provides[i].revision) return 0;
    for (size_t i=0;i<a->require_count;i++) if (strcmp(a->requirements[i].name,b->requirements[i].name) || a->requirements[i].min_revision!=b->requirements[i].min_revision || a->requirements[i].max_revision!=b->requirements[i].max_revision) return 0;
    for (size_t i=0;i<a->conflict_count;i++) if (strcmp(a->conflicts[i],b->conflicts[i])) return 0;
    for (size_t i=0;i<a->attr_count;i++) {
        const mulib_cdd_attr_t *x=a->attrs+i,*y=b->attrs+i;
        if (strcmp(x->key,y->key) || x->value.kind!=y->value.kind) return 0;
        if (x->value.kind==MULIB_CDD_STRING ? strcmp(x->value.string,y->value.string)!=0 : x->value.integer!=y->value.integer) return 0;
    }
    return 1;
}
static void mulib_cdd_diff_emit_(mulib_cdd_change_t *out,size_t cap,size_t *n,mulib_cdd_change_kind_t kind,size_t a,size_t b) {
    if (*n<cap) { out[*n].kind=kind; out[*n].before=a; out[*n].after=b; } (*n)++;
}
mulib_cdd_status_t mulib_cdd_diff(const mulib_cdd_catalogue_t *a,const mulib_cdd_catalogue_t *b,mulib_cdd_change_t *out,size_t cap,size_t *needed) {
    if (!needed) return MULIB_CDD_INVALID;
    *needed=0;
    if (!out && cap) return MULIB_CDD_INVALID;
    mulib_cdd_status_t s=mulib_cdd_validate(a,NULL); if (s) return s; s=mulib_cdd_validate(b,NULL); if (s) return s;
    size_t n=0;
    for (size_t i=0;i<a->count;i++) {
        size_t j=mulib_cdd_find(b,a->entries[i].id);
        if (j==MULIB_CDD_NONE) mulib_cdd_diff_emit_(out,cap,&n,MULIB_CDD_REMOVED,i,j);
        else if (!mulib_cdd_diff_equal_(a->entries+i,b->entries+j)) mulib_cdd_diff_emit_(out,cap,&n,MULIB_CDD_CHANGED,i,j);
    }
    for (size_t j=0;j<b->count;j++) if (mulib_cdd_find(a,b->entries[j].id)==MULIB_CDD_NONE) mulib_cdd_diff_emit_(out,cap,&n,MULIB_CDD_ADDED,MULIB_CDD_NONE,j);
    *needed=n; return n>cap?MULIB_CDD_LIMIT:MULIB_CDD_OK;
}
#ifdef __cplusplus
}
#endif
#endif
