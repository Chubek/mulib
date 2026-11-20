/** @file mulib-asset-catalog.h
 * Caller-backed asset catalogue with identity and substring query.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Linear, case-sensitive search; strings borrowed.
 */
#ifndef MULIB_ASSET_CATALOG_H
#define MULIB_ASSET_CATALOG_H
#include "mulib-asset-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline size_t mulib_asset_catalog_find(const mulib_asset_record *a,size_t n,mulib_asset_id id){if(a)for(size_t i=0;i<n;i++)if(mulib_asset_id_equal(a[i].id,id))return i;return SIZE_MAX;}
static inline bool mulib_asset_catalog_add(mulib_asset_record *a,size_t *n,size_t cap,mulib_asset_record v){if(!a||!n||*n>=cap||!mulib_asset_record_valid(v)||mulib_asset_catalog_find(a,*n,v.id)!=SIZE_MAX)return false;a[(*n)++]=v;return true;}
static inline size_t mulib_asset_catalog_query(const mulib_asset_record *a,size_t n,const char *q,uint32_t kind,size_t *out,size_t cap){if(!a||!q)return 0;size_t k=0;for(size_t i=0;i<n;i++)if((!kind||a[i].kind==kind)&&a[i].label&&strstr(a[i].label,q)){if(out&&k<cap)out[k]=i;k++;}return k;}
#ifdef __cplusplus
}
#endif
#endif
