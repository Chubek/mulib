/* MuLIB: eda-merge. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_MERGE in exactly one implementation translation unit.
 * Profile: Three-way record-level merging; conflicting edits to the same record reject rather than perform field-level merge.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_MERGE_H
#define MULIB_EDA_MERGE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-graph-core.h"
#include "mulib-eda-diff.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_merge(const mulib_eda_design_store *base,const mulib_eda_design_store *left,const mulib_eda_design_store *right,mulib_eda_design_store *out,uint64_t *conflict);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_MERGE) && !defined(MULIB_EDA_MERGE_H_IMPLEMENTED)
#define MULIB_EDA_MERGE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_merge(const mulib_eda_design_store *base,const mulib_eda_design_store *left,const mulib_eda_design_store *right,mulib_eda_design_store *out,uint64_t *conflict){if(!base||!left||!right||!out||out->v)return 0;mulib_eda_changeset l={0},r={0};mulib_eda_design_store t={0};int ok=mulib_eda_diff(base,left,&l)&&mulib_eda_diff(base,right,&r)&&mulib_eda_store_clone(base,&t);if(!ok)goto end;for(size_t i=0;i<l.n;i++)for(size_t j=0;j<r.n;j++)if(l.v[i].record.id==r.v[j].record.id){if(l.v[i].remove!=r.v[j].remove||(!l.v[i].remove&&!mulib_eda_record_equal(&l.v[i].record,&r.v[j].record))){if(conflict)*conflict=l.v[i].record.id;ok=0;goto end;}}ok=mulib_eda_changeset_apply(&l,&t);if(ok){r.base=t.revision;for(size_t i=0;i<r.n;i++)if(r.v[i].remove&&mulib_eda_store_find(&t,r.v[i].record.id)==MULIB_STORAGE_NONE){r.v[i]=r.v[--r.n];i--;}ok=mulib_eda_changeset_apply(&r,&t);}end:mulib_eda_changeset_free(&l);mulib_eda_changeset_free(&r);if(ok)*out=t;else mulib_eda_store_free(&t);return ok;}
#ifdef __cplusplus
}
#endif
#endif
