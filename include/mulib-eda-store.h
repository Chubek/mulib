/* MuLIB: eda-store. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_STORE in exactly one implementation translation unit.
 * Profile: Sorted in-memory typed scalar objects; lookup logarithmic, insert/remove linear. Use eda-serialize/journal for persistence. Not a disk B-tree or general object graph store.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_STORE_H
#define MULIB_EDA_STORE_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id,parent;uint32_t kind,flags;double value;mulib_geom2_point position;} mulib_eda_design_record;
typedef struct {mulib_eda_design_record *v;size_t n,cap;uint64_t revision;} mulib_eda_design_store;
size_t mulib_eda_store_find(const mulib_eda_design_store *s,uint64_t id);
int mulib_eda_store_put(mulib_eda_design_store *s,mulib_eda_design_record r);
int mulib_eda_store_remove(mulib_eda_design_store *s,uint64_t id);
int mulib_eda_store_clone(const mulib_eda_design_store *s,mulib_eda_design_store *out);
void mulib_eda_store_free(mulib_eda_design_store *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_STORE) && !defined(MULIB_EDA_STORE_H_IMPLEMENTED)
#define MULIB_EDA_STORE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_eda_store_find(const mulib_eda_design_store *s,uint64_t id){if(!s)return MULIB_STORAGE_NONE;size_t lo=0,hi=s->n;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(s->v[mid].id<id)lo=mid+1;else hi=mid;}return lo<s->n&&s->v[lo].id==id?lo:MULIB_STORAGE_NONE;}
int mulib_eda_store_put(mulib_eda_design_store *s,mulib_eda_design_record r){if(!s||!r.id||!r.kind||!isfinite(r.value)||!mulib_geom2_point_valid(r.position)||s->revision==UINT64_MAX)return 0;size_t i=mulib_eda_store_find(s,r.id);if(i==MULIB_STORAGE_NONE){if(s->n==SIZE_MAX||!mulib_storage_reserve((void**)&s->v,&s->cap,s->n+1,sizeof r))return 0;size_t lo=0,hi=s->n;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(s->v[mid].id<r.id)lo=mid+1;else hi=mid;}memmove(s->v+lo+1,s->v+lo,(s->n-lo)*sizeof r);s->v[lo]=r;s->n++;}else s->v[i]=r;s->revision++;return 1;}
int mulib_eda_store_remove(mulib_eda_design_store *s,uint64_t id){size_t i=mulib_eda_store_find(s,id);if(i==MULIB_STORAGE_NONE||s->revision==UINT64_MAX)return 0;memmove(s->v+i,s->v+i+1,(s->n-i-1)*sizeof *s->v);s->n--;s->revision++;return 1;}
int mulib_eda_store_clone(const mulib_eda_design_store *s,mulib_eda_design_store *out){if(!s||!out||s==out||out->v||out->n||out->cap)return 0;mulib_eda_design_store t={0};if(!mulib_storage_reserve((void**)&t.v,&t.cap,s->n,sizeof *t.v))return 0;if(s->n)memcpy(t.v,s->v,s->n*sizeof *s->v);t.n=s->n;t.revision=s->revision;*out=t;return 1;}
void mulib_eda_store_free(mulib_eda_design_store *s){if(s){free(s->v);memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
