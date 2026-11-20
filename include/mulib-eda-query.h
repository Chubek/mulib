/* MuLIB: eda-query. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_QUERY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_QUERY_H
#define MULIB_EDA_QUERY_H
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
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_eda_query(const mulib_eda_design_store *s,uint32_t kind,uint64_t parent,mulib_geom2_box *bounds,uint64_t *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_QUERY) && !defined(MULIB_EDA_QUERY_H_IMPLEMENTED)
#define MULIB_EDA_QUERY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_eda_query(const mulib_eda_design_store *s,uint32_t kind,uint64_t parent,mulib_geom2_box *bounds,uint64_t *out,size_t cap){if(!s||(bounds&&!mulib_geom2_box_valid(*bounds)))return MULIB_STORAGE_NONE;size_t n=0;for(size_t i=0;i<s->n;i++){mulib_eda_design_record r=s->v[i];if((kind&&r.kind!=kind)||(parent&&r.parent!=parent))continue;if(bounds&&(r.position.x<bounds->lo.x||r.position.x>bounds->hi.x||r.position.y<bounds->lo.y||r.position.y>bounds->hi.y))continue;if(out&&n<cap)out[n]=r.id;n++;}return n;}
#ifdef __cplusplus
}
#endif
#endif
