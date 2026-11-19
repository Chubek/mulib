/* MuLIB / Amb3r: eda-diff. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_DIFF in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_DIFF_H
#define MULIB_EDA_DIFF_H
#include "mulib-amb3r-base.h"
#include "mulib-eda-changeset.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_record_equal(const ma_design_record *a,const ma_design_record *b);
int mulib_eda_diff(const ma_design_store *a,const ma_design_store *b,ma_changeset *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_DIFF) && !defined(MULIB_EDA_DIFF_H_IMPLEMENTED)
#define MULIB_EDA_DIFF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_record_equal(const ma_design_record *a,const ma_design_record *b){return a&&b&&a->id==b->id&&a->parent==b->parent&&a->kind==b->kind&&a->flags==b->flags&&a->value==b->value&&a->position.x==b->position.x&&a->position.y==b->position.y;}
int mulib_eda_diff(const ma_design_store *a,const ma_design_store *b,ma_changeset *out){if(!a||!b||!out||out->v)return 0;ma_changeset t={0};t.base=a->revision;for(size_t i=0;i<a->n;i++)if(mulib_eda_store_find(b,a->v[i].id)==MA_NONE){ma_design_change c={a->v[i],1};if(!mulib_eda_changeset_push(&t,c))goto fail;}for(size_t i=0;i<b->n;i++){size_t j=mulib_eda_store_find(a,b->v[i].id);if(j==MA_NONE||!mulib_eda_record_equal(a->v+j,b->v+i)){ma_design_change c={b->v[i],0};if(!mulib_eda_changeset_push(&t,c))goto fail;}}*out=t;return 1;fail:mulib_eda_changeset_free(&t);return 0;}
#ifdef __cplusplus
}
#endif
#endif
