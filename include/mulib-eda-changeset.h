/* MuLIB: eda-changeset. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_CHANGESET in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_CHANGESET_H
#define MULIB_EDA_CHANGESET_H
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
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_eda_design_record record;unsigned remove;} mulib_eda_design_change;
typedef struct {mulib_eda_design_change *v;size_t n,cap;uint64_t base;} mulib_eda_changeset;
int mulib_eda_changeset_push(mulib_eda_changeset *c,mulib_eda_design_change change);
int mulib_eda_changeset_apply(const mulib_eda_changeset *c,mulib_eda_design_store *s);
void mulib_eda_changeset_free(mulib_eda_changeset *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_CHANGESET) && !defined(MULIB_EDA_CHANGESET_H_IMPLEMENTED)
#define MULIB_EDA_CHANGESET_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_changeset_push(mulib_eda_changeset *c,mulib_eda_design_change change){if(!c||!change.record.id||change.remove>1)return 0;return mulib_storage_push((void**)&c->v,&c->n,&c->cap,sizeof change,&change);}
int mulib_eda_changeset_apply(const mulib_eda_changeset *c,mulib_eda_design_store *s){if(!c||!s||s->revision!=c->base)return 0;mulib_eda_design_store t={0};if(!mulib_eda_store_clone(s,&t))return 0;for(size_t i=0;i<c->n;i++){int ok=c->v[i].remove?mulib_eda_store_remove(&t,c->v[i].record.id):mulib_eda_store_put(&t,c->v[i].record);if(!ok){mulib_eda_store_free(&t);return 0;}}mulib_eda_store_free(s);*s=t;return 1;}
void mulib_eda_changeset_free(mulib_eda_changeset *c){if(c){free(c->v);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
